// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/origin_chip_view.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/elide_url.h"
#include "chrome/browser/ui/location_bar/origin_chip_info.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_utils.h"
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
          extensions::util::GetDefaultAppIcon(),
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

const extensions::Extension* GetExtension(const GURL& url, Profile* profile) {
  if (!url.SchemeIs(extensions::kExtensionScheme))
    return NULL;
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  return service->extensions()->GetExtensionOrAppByURL(url);
}

}  // namespace

OriginChipView::OriginChipView(LocationBarView* location_bar_view,
                               Profile* profile,
                               const gfx::FontList& font_list)
    : LabelButton(this, base::string16()),
      location_bar_view_(location_bar_view),
      profile_(profile),
      showing_16x16_icon_(false),
      fade_in_animation_(this),
      security_level_(ToolbarModel::NONE),
      url_malware_(false) {
  EnableCanvasFlippingForRTLUI(true);

  scoped_refptr<SafeBrowsingService> sb_service =
      g_browser_process->safe_browsing_service();
  // |sb_service| may be NULL in tests.
  if (sb_service && sb_service->ui_manager())
    sb_service->ui_manager()->AddObserver(this);

  SetFontList(font_list);

  // TODO(gbillock): Would be nice to just use stock LabelButton stuff here.
  location_icon_view_ = new LocationIconView(location_bar_view_);
  // Make location icon hover events count as hovering the origin chip.
  location_icon_view_->set_interactive(false);
  location_icon_view_->ShowTooltip(true);
  AddChildView(location_icon_view_);

  ev_label_ = new views::Label(base::string16(), GetFontList());
  ev_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  ev_label_->SetElideBehavior(gfx::NO_ELIDE);
  AddChildView(ev_label_);

  host_label_ = new views::Label(base::string16(), GetFontList());
  host_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  host_label_->SetElideBehavior(gfx::NO_ELIDE);
  AddChildView(host_label_);

  fade_in_animation_.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
  fade_in_animation_.SetSlideDuration(175);

  // Ensure |pressed_text_color_| and |background_colors_| are initialized.
  SetBorderImages(kNormalImages);
}

OriginChipView::~OriginChipView() {
  scoped_refptr<SafeBrowsingService> sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service.get() && sb_service->ui_manager())
    sb_service->ui_manager()->RemoveObserver(this);
}

void OriginChipView::OnChanged() {
  content::WebContents* web_contents = location_bar_view_->GetWebContents();
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

  ev_label_->SetText(location_bar_view_->GetToolbarModel()->GetEVCertName());
  ev_label_->SetVisible(security_level_ == ToolbarModel::EV_SECURE);

  // TODO(pkasting): Allow the origin chip to shrink, and use ElideHost().
  base::string16 host =
      OriginChip::LabelFromURLForProfile(url_displayed_, profile_);
  host_label_->SetText(host);
  host_label_->SetTooltipText(base::UTF8ToUTF16(url.spec()));

  showing_16x16_icon_ = url_displayed_.is_empty() ||
      url_displayed_.SchemeIs(content::kChromeUIScheme);
  int icon = showing_16x16_icon_ ? IDR_PRODUCT_LOGO_16 :
      location_bar_view_->GetToolbarModel()->GetIconForSecurityLevel(
          security_level_);
  const extensions::Extension* extension =
      GetExtension(url_displayed_, profile_);
  if (extension) {
    icon = IDR_EXTENSIONS_FAVICON;
    showing_16x16_icon_ = true;
    extension_icon_.reset(
        new OriginChipExtensionIcon(location_icon_view_, profile_, extension));
  } else {
    extension_icon_.reset();
  }
  location_icon_view_->SetImage(GetThemeProvider()->GetImageSkiaNamed(icon));

  if (visible()) {
    CancelFade();
    Layout();
    SchedulePaint();
  }
}

void OriginChipView::FadeIn() {
  fade_in_animation_.Show();
}

void OriginChipView::CancelFade() {
  fade_in_animation_.Stop();
}

int OriginChipView::HostLabelOffset() const {
  return host_label_->x() - GetLabelX();
}

int OriginChipView::WidthFromStartOfLabels() const {
  return width() - GetLabelX();
}

gfx::Size OriginChipView::GetPreferredSize() const {
  // TODO(pkasting): Use of " " here is a horrible hack, to be replaced by
  // splitting the chip into separate pieces for EV/host.
  int label_size = host_label_->GetPreferredSize().width();
  if (ev_label_->visible()) {
    label_size += ev_label_->GetPreferredSize().width() +
        gfx::GetStringWidth(base::ASCIIToUTF16(" "), GetFontList());
  }
  return gfx::Size(GetLabelX() + label_size + kEdgeThickness,
                   location_icon_view_->GetPreferredSize().height());
}

void OriginChipView::Layout() {
  // TODO(gbillock): Eventually we almost certainly want to use
  // LocationBarLayout for leading and trailing decorations.

  location_icon_view_->SetBounds(
      kEdgeThickness + (showing_16x16_icon_ ? k16x16IconLeadingSpacing : 0),
      LocationBarView::kNormalEdgeThickness,
      location_icon_view_->GetPreferredSize().width(),
      height() - 2 * LocationBarView::kNormalEdgeThickness);

  int label_x = GetLabelX();
  int label_width = std::max(0, width() - label_x - kEdgeThickness);
  const int label_y = LocationBarView::kNormalEdgeThickness;
  const int label_height = height() - 2 * LocationBarView::kNormalEdgeThickness;
  if (ev_label_->visible()) {
    int ev_label_width =
        std::min(ev_label_->GetPreferredSize().width(), label_width);
    ev_label_->SetBounds(label_x, label_y, ev_label_width, label_height);
    // TODO(pkasting): See comments in GetPreferredSize().
    ev_label_width +=
        gfx::GetStringWidth(base::ASCIIToUTF16(" "), GetFontList());
    label_x += ev_label_width;
    label_width = std::max(0, label_width - ev_label_width);
  }
  host_label_->SetBounds(label_x, label_y, label_width, label_height);
}

int OriginChipView::GetLabelX() const {
  const int icon_spacing = showing_16x16_icon_ ?
      (k16x16IconLeadingSpacing + k16x16IconTrailingSpacing) : 0;
  return kEdgeThickness + location_icon_view_->GetPreferredSize().width() +
      icon_spacing + kIconTextSpacing;
}

void OriginChipView::SetBorderImages(const int images[3][9]) {
  scoped_ptr<views::LabelButtonBorder> border(
      new views::LabelButtonBorder(views::Button::STYLE_BUTTON));

  for (size_t i = 0; i < 3; ++i) {
    views::Painter* painter = views::Painter::CreateImageGridPainter(images[i]);
    border->SetPainter(false, static_cast<Button::ButtonState>(i), painter);

    // Calculate a representative background color of the provided image grid to
    // use as the background color of the host label in order to color the text
    // appropriately. We grab the color of the middle pixel of the middle image
    // of the background, which we treat as the representative color of the
    // entire background (reasonable, given the current appearance of these
    // images).
    //
    // NOTE: Because this is called from the constructor, when we're not in a
    // Widget yet, GetThemeProvider() may return NULL, so use the location bar's
    // theme provider instead to be safe.
    const SkBitmap& bitmap(
        location_bar_view_->GetThemeProvider()->GetImageSkiaNamed(
            images[i][4])->GetRepresentation(1.0f).sk_bitmap());
    SkAutoLockPixels pixel_lock(bitmap);
    background_colors_[i] =
        bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2);
  }

  // Calculate the actual text color of the pressed label.
  host_label_->SetBackgroundColor(background_colors_[Button::STATE_PRESSED]);
  pressed_text_color_ = host_label_->enabled_color();
  host_label_->SetBackgroundColor(background_colors_[state()]);

  SetBorder(border.PassAs<views::Border>());
}

void OriginChipView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == &fade_in_animation_)
    SchedulePaint();
  else
    views::LabelButton::AnimationProgressed(animation);
}

void OriginChipView::AnimationEnded(const gfx::Animation* animation) {
  if (animation == &fade_in_animation_)
    fade_in_animation_.Reset();
  else
    views::LabelButton::AnimationEnded(animation);
}

void OriginChipView::OnPaintBorder(gfx::Canvas* canvas) {
  if (fade_in_animation_.is_animating()) {
    canvas->SaveLayerAlpha(static_cast<uint8>(
        fade_in_animation_.CurrentValueBetween(0, 255)));
    views::LabelButton::OnPaintBorder(canvas);
    canvas->Restore();
  } else {
    views::LabelButton::OnPaintBorder(canvas);
  }
}

void OriginChipView::StateChanged() {
  DCHECK_LT(state(), 3);
  SkColor background_color = background_colors_[state()];
  ev_label_->SetBackgroundColor(background_color);
  host_label_->SetBackgroundColor(background_color);
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
