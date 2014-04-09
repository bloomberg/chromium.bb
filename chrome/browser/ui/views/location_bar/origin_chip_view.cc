// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/origin_chip_view.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_icon_image.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/elide_url.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/toolbar/origin_chip_info.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/painter.h"


// OriginChipExtensionIcon ----------------------------------------------------

class OriginChipExtensionIcon : public extensions::IconImage::Observer {
 public:
  OriginChipExtensionIcon(LocationIconView* icon_view,
                          Profile* profile,
                          const extensions::Extension* extension);
  virtual ~OriginChipExtensionIcon();

  // IconImage::Observer:
  virtual void OnExtensionIconImageChanged(
      extensions::IconImage* image) OVERRIDE;

 private:
  LocationIconView* icon_view_;
  scoped_ptr<extensions::IconImage> icon_image_;

  DISALLOW_COPY_AND_ASSIGN(OriginChipExtensionIcon);
};

OriginChipExtensionIcon::OriginChipExtensionIcon(
    LocationIconView* icon_view,
    Profile* profile,
    const extensions::Extension* extension)
    : icon_view_(icon_view),
      icon_image_(new extensions::IconImage(
          profile,
          extension,
          extensions::IconsInfo::GetIcons(extension),
          extension_misc::EXTENSION_ICON_BITTY,
          extensions::IconsInfo::GetDefaultAppIcon(),
          this)) {
  // Forces load of the image.
  icon_image_->image_skia().GetRepresentation(1.0f);

  if (!icon_image_->image_skia().image_reps().empty())
    OnExtensionIconImageChanged(icon_image_.get());
}

OriginChipExtensionIcon::~OriginChipExtensionIcon() {
}

void OriginChipExtensionIcon::OnExtensionIconImageChanged(
    extensions::IconImage* image) {
  if (icon_view_)
    icon_view_->SetImage(&icon_image_->image_skia());
}


// OriginChipView -------------------------------------------------------------

namespace {

const int kEdgeThickness = 5;
const int k16x16IconLeadingSpacing = 1;
const int k16x16IconTrailingSpacing = 2;
const int kIconTextSpacing = 3;
const int kTrailingLabelMargin = 0;

const int kNormalImages[3][9] = {
  IMAGE_GRID(IDR_ORIGIN_CHIP_NORMAL),
  IMAGE_GRID(IDR_ORIGIN_CHIP_HOVER),
  IMAGE_GRID(IDR_ORIGIN_CHIP_PRESSED)
};

const int kMalwareImages[3][9] = {
  IMAGE_GRID(IDR_ORIGIN_CHIP_MALWARE_NORMAL),
  IMAGE_GRID(IDR_ORIGIN_CHIP_MALWARE_HOVER),
  IMAGE_GRID(IDR_ORIGIN_CHIP_MALWARE_PRESSED)
};

const int kBrokenSSLImages[3][9] = {
  IMAGE_GRID(IDR_ORIGIN_CHIP_BROKENSSL_NORMAL),
  IMAGE_GRID(IDR_ORIGIN_CHIP_BROKENSSL_HOVER),
  IMAGE_GRID(IDR_ORIGIN_CHIP_BROKENSSL_PRESSED)
};

const int kEVImages[3][9] = {
  IMAGE_GRID(IDR_ORIGIN_CHIP_EV_NORMAL),
  IMAGE_GRID(IDR_ORIGIN_CHIP_EV_HOVER),
  IMAGE_GRID(IDR_ORIGIN_CHIP_EV_PRESSED)
};

}  // namespace

OriginChipView::OriginChipView(LocationBarView* location_bar_view,
                               Profile* profile,
                               const gfx::FontList& font_list)
    : LabelButton(this, base::string16()),
      location_bar_view_(location_bar_view),
      profile_(profile),
      showing_16x16_icon_(false) {
  scoped_refptr<SafeBrowsingService> sb_service =
      g_browser_process->safe_browsing_service();
  // May not be set for unit tests.
  if (sb_service && sb_service->ui_manager())
    sb_service->ui_manager()->AddObserver(this);

  SetFontList(font_list);

  image()->EnableCanvasFlippingForRTLUI(false);

  // TODO(gbillock): Would be nice to just use stock LabelButton stuff here.
  location_icon_view_ = new LocationIconView(location_bar_view_);
  // Make location icon hover events count as hovering the origin chip.
  location_icon_view_->set_interactive(false);
  location_icon_view_->ShowTooltip(true);
  AddChildView(location_icon_view_);

  host_label_ = new views::Label(base::string16(), GetFontList());
  AddChildView(host_label_);

  fade_in_animation_.reset(new gfx::SlideAnimation(this));
  fade_in_animation_->SetTweenType(gfx::Tween::LINEAR);
  fade_in_animation_->SetSlideDuration(300);
}

OriginChipView::~OriginChipView() {
  scoped_refptr<SafeBrowsingService> sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service.get() && sb_service->ui_manager())
    sb_service->ui_manager()->RemoveObserver(this);
}

bool OriginChipView::ShouldShow() {
  return chrome::ShouldDisplayOriginChipV2() &&
      location_bar_view_->GetToolbarModel()->WouldOmitURLDueToOriginChip() &&
      location_bar_view_->GetToolbarModel()->origin_chip_enabled();
}

void OriginChipView::Update(content::WebContents* web_contents) {
  if (!web_contents)
    return;

  // Note: security level can change async as the connection is made.
  GURL url = location_bar_view_->GetToolbarModel()->GetURL();
  const ToolbarModel::SecurityLevel security_level =
      location_bar_view_->GetToolbarModel()->GetSecurityLevel(true);

  bool url_malware = OriginChip::IsMalware(url, web_contents);

  // TODO(gbillock): We persist a malware setting while a new WebContents
  // content is loaded, meaning that we end up transiently marking a safe
  // page as malware. Need to fix that.

  if ((url == url_displayed_) &&
      (security_level == security_level_) &&
      (url_malware == url_malware_))
    return;

  url_displayed_ = url;
  url_malware_ = url_malware;
  security_level_ = security_level;

  if (url_malware_) {
    SetBorderImages(kMalwareImages);
  } else if (security_level_ == ToolbarModel::SECURITY_ERROR) {
    SetBorderImages(kBrokenSSLImages);
  } else if (security_level_ == ToolbarModel::EV_SECURE) {
    SetBorderImages(kEVImages);
  } else {
    SetBorderImages(kNormalImages);
  }

  base::string16 host =
      OriginChip::LabelFromURLForProfile(url_displayed_, profile_);
  if (security_level_ == ToolbarModel::EV_SECURE) {
    host = l10n_util::GetStringFUTF16(IDS_SITE_CHIP_EV_SSL_LABEL,
        location_bar_view_->GetToolbarModel()->GetEVCertName(),
        host);
  }
  host_label_->SetText(host);
  host_label_->SetTooltipText(host);
  host_label_->SetElideBehavior(views::Label::NO_ELIDE);

  int icon = location_bar_view_->GetToolbarModel()->GetIconForSecurityLevel(
      security_level_);
  showing_16x16_icon_ = false;

  if (url_displayed_.is_empty() ||
      url_displayed_.SchemeIs(content::kChromeUIScheme)) {
    icon = IDR_PRODUCT_LOGO_16;
    showing_16x16_icon_ = true;
  }

  location_icon_view_->SetImage(GetThemeProvider()->GetImageSkiaNamed(icon));

  if (url_displayed_.SchemeIs(extensions::kExtensionScheme)) {
    icon = IDR_EXTENSIONS_FAVICON;
    showing_16x16_icon_ = true;
    location_icon_view_->SetImage(GetThemeProvider()->GetImageSkiaNamed(icon));

    ExtensionService* service =
        extensions::ExtensionSystem::Get(profile_)->extension_service();
    const extensions::Extension* extension =
        service->extensions()->GetExtensionOrAppByURL(url_displayed_);
    extension_icon_.reset(
        new OriginChipExtensionIcon(location_icon_view_, profile_, extension));
  } else {
    extension_icon_.reset();
  }

  Layout();
  SchedulePaint();
}

void OriginChipView::OnChanged() {
  Update(location_bar_view_->GetWebContents());
  // TODO(gbillock): Also need to potentially repaint infobars to make sure the
  // arrows are pointing to the right spot. Only needed for some edge cases.
}

int OriginChipView::ElideDomainTarget(int target_max_width) {
  base::string16 host =
      OriginChip::LabelFromURLForProfile(url_displayed_, profile_);
  host_label_->SetText(host);
  int width = GetPreferredSize().width();
  if (width <= target_max_width)
    return width;

  gfx::Size label_size = host_label_->GetPreferredSize();
  int padding_width = width - label_size.width();

  host_label_->SetText(ElideHost(
      location_bar_view_->GetToolbarModel()->GetURL(),
      host_label_->font_list(), target_max_width - padding_width));
  return GetPreferredSize().width();
}

void OriginChipView::FadeIn() {
  fade_in_animation_->Show();
}

gfx::Size OriginChipView::GetPreferredSize() {
  gfx::Size label_size = host_label_->GetPreferredSize();
  gfx::Size icon_size = location_icon_view_->GetPreferredSize();
  int icon_spacing = showing_16x16_icon_ ?
      (k16x16IconLeadingSpacing + k16x16IconTrailingSpacing) : 0;
  return gfx::Size(kEdgeThickness + icon_size.width() + icon_spacing +
                   kIconTextSpacing + label_size.width() +
                   kTrailingLabelMargin + kEdgeThickness,
                   icon_size.height());
}

void OriginChipView::SetBorderImages(const int images[3][9]) {
  scoped_ptr<views::LabelButtonBorder> border(
      new views::LabelButtonBorder(views::Button::STYLE_BUTTON));

  views::Painter* painter = views::Painter::CreateImageGridPainter(images[0]);
  border->SetPainter(false, Button::STATE_NORMAL, painter);
  painter = views::Painter::CreateImageGridPainter(images[1]);
  border->SetPainter(false, Button::STATE_HOVERED, painter);
  painter = views::Painter::CreateImageGridPainter(images[2]);
  border->SetPainter(false, Button::STATE_PRESSED, painter);

  SetBorder(border.PassAs<views::Border>());

  // Calculate a representative background color of the provided image grid and
  // set it as the background color of the host label in order to color the text
  // appropriately. We grab the color of the middle pixel of the middle image
  // of the background, which we treat as the representative color of the entire
  // background (reasonable, given the current appearance of these images).
  const SkBitmap& bitmap(
      GetThemeProvider()->GetImageSkiaNamed(
          images[0][4])->GetRepresentation(1.0f).sk_bitmap());
  SkAutoLockPixels pixel_lock(bitmap);
  host_label_->SetBackgroundColor(
      bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
}

void OriginChipView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == fade_in_animation_.get())
    SchedulePaint();
  else
    views::LabelButton::AnimationProgressed(animation);
}

void OriginChipView::AnimationEnded(const gfx::Animation* animation) {
  if (animation == fade_in_animation_.get())
    fade_in_animation_->Reset();
  else
    views::LabelButton::AnimationEnded(animation);
}

void OriginChipView::Layout() {
  // TODO(gbillock): Eventually we almost certainly want to use
  // LocationBarLayout for leading and trailing decorations.

  location_icon_view_->SetBounds(
      kEdgeThickness + (showing_16x16_icon_ ? k16x16IconLeadingSpacing : 0),
      LocationBarView::kNormalEdgeThickness,
      location_icon_view_->GetPreferredSize().width(),
      height() - 2 * LocationBarView::kNormalEdgeThickness);

  int host_label_x = location_icon_view_->x() + location_icon_view_->width() +
      kIconTextSpacing;
  host_label_x += showing_16x16_icon_ ? k16x16IconTrailingSpacing : 0;
  int host_label_width =
      width() - host_label_x - kEdgeThickness - kTrailingLabelMargin;
  host_label_->SetBounds(host_label_x,
                         LocationBarView::kNormalEdgeThickness,
                         host_label_width,
                         height() - 2 * LocationBarView::kNormalEdgeThickness);
}

void OriginChipView::OnPaintBorder(gfx::Canvas* canvas) {
  if (fade_in_animation_->is_animating()) {
    canvas->SaveLayerAlpha(static_cast<uint8>(
        fade_in_animation_->CurrentValueBetween(0, 255)));
    views::LabelButton::OnPaintBorder(canvas);
    canvas->Restore();
  } else {
    views::LabelButton::OnPaintBorder(canvas);
  }
}

// TODO(gbillock): Make the LocationBarView or OmniboxView the listener for
// this button.
void OriginChipView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  // See if the event needs to be passed to the LocationIconView.
  if (event.IsMouseEvent() || (event.type() == ui::ET_GESTURE_TAP)) {
    location_icon_view_->set_interactive(true);
    const ui::LocatedEvent& located_event =
        static_cast<const ui::LocatedEvent&>(event);
    if (GetEventHandlerForPoint(located_event.location()) ==
            location_icon_view_) {
      location_icon_view_->page_info_helper()->ProcessEvent(located_event);
      location_icon_view_->set_interactive(false);
      return;
    }
    location_icon_view_->set_interactive(false);
  }

  UMA_HISTOGRAM_COUNTS("OriginChip.Pressed", 1);
  content::RecordAction(base::UserMetricsAction("OriginChipPress"));

  location_bar_view_->ShowURL();
}

// Note: When OnSafeBrowsingHit would be called, OnSafeBrowsingMatch will
// have already been called.
void OriginChipView::OnSafeBrowsingHit(
    const SafeBrowsingUIManager::UnsafeResource& resource) {}

void OriginChipView::OnSafeBrowsingMatch(
    const SafeBrowsingUIManager::UnsafeResource& resource) {
  OnChanged();
}
