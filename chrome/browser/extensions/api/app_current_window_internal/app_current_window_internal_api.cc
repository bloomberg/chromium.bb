// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/app_current_window_internal/app_current_window_internal_api.h"

#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "apps/size_constraints.h"
#include "apps/ui/native_app_window.h"
#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/app_current_window_internal.h"
#include "chrome/common/extensions/api/app_window.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/switches.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace app_current_window_internal =
    extensions::api::app_current_window_internal;

namespace Show = app_current_window_internal::Show;
namespace SetBounds = app_current_window_internal::SetBounds;
namespace SetSizeConstraints = app_current_window_internal::SetSizeConstraints;
namespace SetIcon = app_current_window_internal::SetIcon;
namespace SetBadgeIcon = app_current_window_internal::SetBadgeIcon;
namespace SetShape = app_current_window_internal::SetShape;
namespace SetAlwaysOnTop = app_current_window_internal::SetAlwaysOnTop;

using apps::AppWindow;
using app_current_window_internal::Bounds;
using app_current_window_internal::Region;
using app_current_window_internal::RegionRect;
using app_current_window_internal::SizeConstraints;

namespace extensions {

namespace {

const char kNoAssociatedAppWindow[] =
    "The context from which the function was called did not have an "
    "associated app window.";

const char kDevChannelOnly[] =
    "This function is currently only available in the Dev channel.";

const char kRequiresFramelessWindow[] =
    "This function requires a frameless window (frame:none).";

const char kAlwaysOnTopPermission[] =
    "The \"app.window.alwaysOnTop\" permission is required.";

const char kInvalidParameters[] = "Invalid parameters.";

const int kUnboundedSize = apps::SizeConstraints::kUnboundedSize;

void GetBoundsFields(const Bounds& bounds_spec, gfx::Rect* bounds) {
  if (bounds_spec.left)
    bounds->set_x(*bounds_spec.left);
  if (bounds_spec.top)
    bounds->set_y(*bounds_spec.top);
  if (bounds_spec.width)
    bounds->set_width(*bounds_spec.width);
  if (bounds_spec.height)
    bounds->set_height(*bounds_spec.height);
}

// Copy the constraint value from the API to our internal representation of
// content size constraints. A value of zero resets the constraints. The insets
// are used to transform window constraints to content constraints.
void GetConstraintWidth(const scoped_ptr<int>& width,
                        const gfx::Insets& insets,
                        gfx::Size* size) {
  if (!width.get())
    return;

  size->set_width(*width > 0 ? std::max(0, *width - insets.width())
                             : kUnboundedSize);
}

void GetConstraintHeight(const scoped_ptr<int>& height,
                         const gfx::Insets& insets,
                         gfx::Size* size) {
  if (!height.get())
    return;

  size->set_height(*height > 0 ? std::max(0, *height - insets.height())
                               : kUnboundedSize);
}

}  // namespace

namespace bounds {

enum BoundsType {
  INNER_BOUNDS,
  OUTER_BOUNDS,
  DEPRECATED_BOUNDS,
  INVALID_TYPE
};

const char kInnerBoundsType[] = "innerBounds";
const char kOuterBoundsType[] = "outerBounds";
const char kDeprecatedBoundsType[] = "bounds";

BoundsType GetBoundsType(const std::string& type_as_string) {
  if (type_as_string == kInnerBoundsType)
    return INNER_BOUNDS;
  else if (type_as_string == kOuterBoundsType)
    return OUTER_BOUNDS;
  else if (type_as_string == kDeprecatedBoundsType)
    return DEPRECATED_BOUNDS;
  else
    return INVALID_TYPE;
}

}  // namespace bounds

bool AppCurrentWindowInternalExtensionFunction::RunImpl() {
  apps::AppWindowRegistry* registry =
      apps::AppWindowRegistry::Get(GetProfile());
  DCHECK(registry);
  content::RenderViewHost* rvh = render_view_host();
  if (!rvh)
    // No need to set an error, since we won't return to the caller anyway if
    // there's no RVH.
    return false;
  AppWindow* window = registry->GetAppWindowForRenderViewHost(rvh);
  if (!window) {
    error_ = kNoAssociatedAppWindow;
    return false;
  }
  return RunWithWindow(window);
}

bool AppCurrentWindowInternalFocusFunction::RunWithWindow(AppWindow* window) {
  window->GetBaseWindow()->Activate();
  return true;
}

bool AppCurrentWindowInternalFullscreenFunction::RunWithWindow(
    AppWindow* window) {
  window->Fullscreen();
  return true;
}

bool AppCurrentWindowInternalMaximizeFunction::RunWithWindow(
    AppWindow* window) {
  window->Maximize();
  return true;
}

bool AppCurrentWindowInternalMinimizeFunction::RunWithWindow(
    AppWindow* window) {
  window->Minimize();
  return true;
}

bool AppCurrentWindowInternalRestoreFunction::RunWithWindow(AppWindow* window) {
  window->Restore();
  return true;
}

bool AppCurrentWindowInternalDrawAttentionFunction::RunWithWindow(
    AppWindow* window) {
  window->GetBaseWindow()->FlashFrame(true);
  return true;
}

bool AppCurrentWindowInternalClearAttentionFunction::RunWithWindow(
    AppWindow* window) {
  window->GetBaseWindow()->FlashFrame(false);
  return true;
}

bool AppCurrentWindowInternalShowFunction::RunWithWindow(AppWindow* window) {
  scoped_ptr<Show::Params> params(Show::Params::Create(*args_));
  CHECK(params.get());
  if (params->focused && !*params->focused)
    window->Show(AppWindow::SHOW_INACTIVE);
  else
    window->Show(AppWindow::SHOW_ACTIVE);
  return true;
}

bool AppCurrentWindowInternalHideFunction::RunWithWindow(AppWindow* window) {
  window->Hide();
  return true;
}

bool AppCurrentWindowInternalSetBoundsFunction::RunWithWindow(
    AppWindow* window) {
  scoped_ptr<SetBounds::Params> params(SetBounds::Params::Create(*args_));
  CHECK(params.get());

  bounds::BoundsType bounds_type = bounds::GetBoundsType(params->bounds_type);
  if (bounds_type == bounds::INVALID_TYPE) {
    NOTREACHED();
    error_ = kInvalidParameters;
    return false;
  }

  // Start with the current bounds, and change any values that are specified in
  // the incoming parameters.
  gfx::Rect original_window_bounds = window->GetBaseWindow()->GetBounds();
  gfx::Rect window_bounds = original_window_bounds;
  gfx::Insets frame_insets = window->GetBaseWindow()->GetFrameInsets();
  const Bounds& bounds_spec = params->bounds;

  switch (bounds_type) {
    case bounds::DEPRECATED_BOUNDS: {
      // We need to maintain backcompatibility with a bug on Windows and
      // ChromeOS, which sets the position of the window but the size of the
      // content.
      if (bounds_spec.left)
        window_bounds.set_x(*bounds_spec.left);
      if (bounds_spec.top)
        window_bounds.set_y(*bounds_spec.top);
      if (bounds_spec.width)
        window_bounds.set_width(*bounds_spec.width + frame_insets.width());
      if (bounds_spec.height)
        window_bounds.set_height(*bounds_spec.height + frame_insets.height());
      break;
    }
    case bounds::OUTER_BOUNDS: {
      GetBoundsFields(bounds_spec, &window_bounds);
      break;
    }
    case bounds::INNER_BOUNDS: {
      window_bounds.Inset(frame_insets);
      GetBoundsFields(bounds_spec, &window_bounds);
      window_bounds.Inset(-frame_insets);
      break;
    }
    default:
      NOTREACHED();
  }

  if (original_window_bounds != window_bounds) {
    if (original_window_bounds.size() != window_bounds.size()) {
      apps::SizeConstraints constraints(
          apps::SizeConstraints::AddFrameToConstraints(
              window->GetBaseWindow()->GetContentMinimumSize(), frame_insets),
          apps::SizeConstraints::AddFrameToConstraints(
              window->GetBaseWindow()->GetContentMaximumSize(), frame_insets));

      window_bounds.set_size(constraints.ClampSize(window_bounds.size()));
    }

    window->GetBaseWindow()->SetBounds(window_bounds);
  }

  return true;
}

bool AppCurrentWindowInternalSetSizeConstraintsFunction::RunWithWindow(
    AppWindow* window) {
  scoped_ptr<SetSizeConstraints::Params> params(
      SetSizeConstraints::Params::Create(*args_));
  CHECK(params.get());

  bounds::BoundsType bounds_type = bounds::GetBoundsType(params->bounds_type);
  if (bounds_type != bounds::INNER_BOUNDS &&
      bounds_type != bounds::OUTER_BOUNDS) {
    NOTREACHED();
    error_ = kInvalidParameters;
    return false;
  }

  gfx::Size original_min_size =
      window->GetBaseWindow()->GetContentMinimumSize();
  gfx::Size original_max_size =
      window->GetBaseWindow()->GetContentMaximumSize();
  gfx::Size min_size = original_min_size;
  gfx::Size max_size = original_max_size;
  const SizeConstraints& constraints = params->constraints;

  // Use the frame insets to convert window size constraints to content size
  // constraints.
  gfx::Insets insets;
  if (bounds_type == bounds::OUTER_BOUNDS)
    insets = window->GetBaseWindow()->GetFrameInsets();

  GetConstraintWidth(constraints.min_width, insets, &min_size);
  GetConstraintWidth(constraints.max_width, insets, &max_size);
  GetConstraintHeight(constraints.min_height, insets, &min_size);
  GetConstraintHeight(constraints.max_height, insets, &max_size);

  if (min_size != original_min_size || max_size != original_max_size)
    window->SetContentSizeConstraints(min_size, max_size);

  return true;
}

bool AppCurrentWindowInternalSetIconFunction::RunWithWindow(AppWindow* window) {
  if (GetCurrentChannel() > chrome::VersionInfo::CHANNEL_DEV &&
      GetExtension()->location() != extensions::Manifest::COMPONENT) {
    error_ = kDevChannelOnly;
    return false;
  }

  scoped_ptr<SetIcon::Params> params(SetIcon::Params::Create(*args_));
  CHECK(params.get());
  // The |icon_url| parameter may be a blob url (e.g. an image fetched with an
  // XMLHttpRequest) or a resource url.
  GURL url(params->icon_url);
  if (!url.is_valid())
    url = GetExtension()->GetResourceURL(params->icon_url);

  window->SetAppIconUrl(url);
  return true;
}

bool AppCurrentWindowInternalSetBadgeIconFunction::RunWithWindow(
    AppWindow* window) {
  if (GetCurrentChannel() > chrome::VersionInfo::CHANNEL_DEV) {
    error_ = kDevChannelOnly;
    return false;
  }

  scoped_ptr<SetBadgeIcon::Params> params(SetBadgeIcon::Params::Create(*args_));
  CHECK(params.get());
  // The |icon_url| parameter may be a blob url (e.g. an image fetched with an
  // XMLHttpRequest) or a resource url.
  GURL url(params->icon_url);
  if (!url.is_valid() && !params->icon_url.empty())
    url = GetExtension()->GetResourceURL(params->icon_url);

  window->SetBadgeIconUrl(url);
  return true;
}

bool AppCurrentWindowInternalClearBadgeFunction::RunWithWindow(
    AppWindow* window) {
  if (GetCurrentChannel() > chrome::VersionInfo::CHANNEL_DEV) {
    error_ = kDevChannelOnly;
    return false;
  }

  window->ClearBadge();
  return true;
}

bool AppCurrentWindowInternalSetShapeFunction::RunWithWindow(
    AppWindow* window) {

  if (!window->GetBaseWindow()->IsFrameless()) {
    error_ = kRequiresFramelessWindow;
    return false;
  }

  const char* whitelist[] = {
    "0F42756099D914A026DADFA182871C015735DD95",  // http://crbug.com/323773
    "2D22CDB6583FD0A13758AEBE8B15E45208B4E9A7",

    "E7E2461CE072DF036CF9592740196159E2D7C089",  // http://crbug.com/356200
    "A74A4D44C7CFCD8844830E6140C8D763E12DD8F3",
    "312745D9BF916161191143F6490085EEA0434997",
    "53041A2FA309EECED01FFC751E7399186E860B2C",

    "EBA908206905323CECE6DC4B276A58A0F4AC573F",
    "2775E568AC98F9578791F1EAB65A1BF5F8CEF414",
    "4AA3C5D69A4AECBD236CAD7884502209F0F5C169",
    "E410CDAB2C6E6DD408D731016CECF2444000A912",
    "9E930B2B5EABA6243AE6C710F126E54688E8FAF6",

    "FAFE8EFDD2D6AE2EEB277AFEB91C870C79064D9E",  // http://crbug.com/327507
    "3B52D273A271D4E2348233E322426DBAE854B567",
    "5DF6ADC8708DF59FCFDDBF16AFBFB451380C2059",
    "1037DEF5F6B06EA46153AD87B6C5C37440E3F2D1",
    "F5815DAFEB8C53B078DD1853B2059E087C42F139",
    "6A08EFFF9C16E090D6DCC7EC55A01CADAE840513",

    "C32D6D93E12F5401DAA3A723E0C3CC5F25429BA4",  // http://crbug.com/354258
    "9099782647D39C778E15C8C6E0D23C88F5CDE170",
    "B7D5B52D1E5B106288BD7F278CAFA5E8D76108B0",
    "89349DBAA2C4022FB244AA50182AB60934EB41EE",
    "CB593E510640572A995CB1B6D41BD85ED51E63F8",
    "1AD1AC86C87969CD3434FA08D99DBA6840AEA612",
    "9C2EA21D7975BDF2B3C01C3A454EE44854067A6D",
    "D2C488C80C3C90C3E01A991112A05E37831E17D0",
    "6EEC061C0E74B46C7B5BE2EEFA49436368F4988F",
    "8B344D9E8A4C505EF82A0DBBC25B8BD1F984E777",
    "E06AFCB1EB0EFD237824CC4AC8FDD3D43E8BC868"
  };
  if (GetCurrentChannel() > chrome::VersionInfo::CHANNEL_DEV &&
      !SimpleFeature::IsIdInWhitelist(
          GetExtension()->id(),
          std::set<std::string>(whitelist,
                                whitelist + arraysize(whitelist)))) {
    error_ = kDevChannelOnly;
    return false;
  }

  scoped_ptr<SetShape::Params> params(
      SetShape::Params::Create(*args_));
  const Region& shape = params->region;

  // Build a region from the supplied list of rects.
  // If |rects| is missing, then the input region is removed. This clears the
  // input region so that the entire window accepts input events.
  // To specify an empty input region (so the window ignores all input),
  // |rects| should be an empty list.
  scoped_ptr<SkRegion> region(new SkRegion);
  if (shape.rects) {
    for (std::vector<linked_ptr<RegionRect> >::const_iterator i =
             shape.rects->begin();
         i != shape.rects->end();
         ++i) {
      const RegionRect& inputRect = **i;
      int32_t x = inputRect.left;
      int32_t y = inputRect.top;
      int32_t width = inputRect.width;
      int32_t height = inputRect.height;

      SkIRect rect = SkIRect::MakeXYWH(x, y, width, height);
      region->op(rect, SkRegion::kUnion_Op);
    }
  } else {
    region.reset(NULL);
  }

  window->UpdateShape(region.Pass());

  return true;
}

bool AppCurrentWindowInternalSetAlwaysOnTopFunction::RunWithWindow(
    AppWindow* window) {
  if (!GetExtension()->HasAPIPermission(
          extensions::APIPermission::kAlwaysOnTopWindows)) {
    error_ = kAlwaysOnTopPermission;
    return false;
  }

  scoped_ptr<SetAlwaysOnTop::Params> params(
      SetAlwaysOnTop::Params::Create(*args_));
  CHECK(params.get());
  window->SetAlwaysOnTop(params->always_on_top);
  return true;
}

}  // namespace extensions
