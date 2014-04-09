// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_origin_chip_view.h"

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
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
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
#include "ui/views/background.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/painter.h"


// ToolbarOriginChipExtensionIcon ---------------------------------------------

class ToolbarOriginChipExtensionIcon : public extensions::IconImage::Observer {
 public:
  ToolbarOriginChipExtensionIcon(LocationIconView* icon_view,
                                 Profile* profile,
                                 const extensions::Extension* extension);
  virtual ~ToolbarOriginChipExtensionIcon();

  // IconImage::Observer:
  virtual void OnExtensionIconImageChanged(
      extensions::IconImage* image) OVERRIDE;

 private:
  LocationIconView* icon_view_;
  scoped_ptr<extensions::IconImage> icon_image_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarOriginChipExtensionIcon);
};

ToolbarOriginChipExtensionIcon::ToolbarOriginChipExtensionIcon(
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

ToolbarOriginChipExtensionIcon::~ToolbarOriginChipExtensionIcon() {
}

void ToolbarOriginChipExtensionIcon::OnExtensionIconImageChanged(
    extensions::IconImage* image) {
  if (icon_view_)
    icon_view_->SetImage(&icon_image_->image_skia());
}


// ToolbarOriginChipView ------------------------------------------------------

namespace {

const int kEdgeThickness = 5;
const int k16x16IconLeadingSpacing = 1;
const int k16x16IconTrailingSpacing = 2;
const int kIconTextSpacing = 3;
const int kTrailingLabelMargin = 0;

const SkColor kEVBackgroundColor = SkColorSetRGB(163, 226, 120);
const SkColor kMalwareBackgroundColor = SkColorSetRGB(145, 0, 0);
const SkColor kBrokenSSLBackgroundColor = SkColorSetRGB(253, 196, 36);

}  // namespace

ToolbarOriginChipView::ToolbarOriginChipView(ToolbarView* toolbar_view)
    : ToolbarButton(this, NULL),
      toolbar_view_(toolbar_view),
      painter_(NULL),
      showing_16x16_icon_(false) {
  scoped_refptr<SafeBrowsingService> sb_service =
      g_browser_process->safe_browsing_service();
  // May not be set for unit tests.
  if (sb_service.get() && sb_service->ui_manager())
    sb_service->ui_manager()->AddObserver(this);

  set_drag_controller(this);
}

ToolbarOriginChipView::~ToolbarOriginChipView() {
  scoped_refptr<SafeBrowsingService> sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service.get() && sb_service->ui_manager())
    sb_service->ui_manager()->RemoveObserver(this);
}

void ToolbarOriginChipView::Init() {
  ToolbarButton::Init();
  image()->EnableCanvasFlippingForRTLUI(false);

  // TODO(gbillock): Would be nice to just use stock LabelButton stuff here.
  location_icon_view_ = new LocationIconView(toolbar_view_->location_bar());
  // Make location icon hover events count as hovering the origin chip.
  location_icon_view_->set_interactive(false);

  host_label_ = new views::Label();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  host_label_->SetFontList(rb.GetFontList(ui::ResourceBundle::MediumFont));

  AddChildView(location_icon_view_);
  AddChildView(host_label_);

  location_icon_view_->SetImage(GetThemeProvider()->GetImageSkiaNamed(
      IDR_LOCATION_BAR_HTTP));
  location_icon_view_->ShowTooltip(true);

  const int kEVBackgroundImages[] = IMAGE_GRID(IDR_SITE_CHIP_EV);
  ev_background_painter_.reset(
      views::Painter::CreateImageGridPainter(kEVBackgroundImages));
  const int kBrokenSSLBackgroundImages[] = IMAGE_GRID(IDR_SITE_CHIP_BROKENSSL);
  broken_ssl_background_painter_.reset(
      views::Painter::CreateImageGridPainter(kBrokenSSLBackgroundImages));
  const int kMalwareBackgroundImages[] = IMAGE_GRID(IDR_SITE_CHIP_MALWARE);
  malware_background_painter_.reset(
      views::Painter::CreateImageGridPainter(kMalwareBackgroundImages));
}

bool ToolbarOriginChipView::ShouldShow() {
  return chrome::ShouldDisplayOriginChip();
}

void ToolbarOriginChipView::Update(content::WebContents* web_contents) {
  if (!web_contents)
    return;

  // Note: security level can change async as the connection is made.
  GURL url = toolbar_view_->GetToolbarModel()->GetURL();
  const ToolbarModel::SecurityLevel security_level =
      toolbar_view_->GetToolbarModel()->GetSecurityLevel(true);

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

  SkColor label_background =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR);
  if (url_malware_) {
    painter_ = malware_background_painter_.get();
    label_background = kMalwareBackgroundColor;
  } else if (security_level_ == ToolbarModel::SECURITY_ERROR) {
    painter_ = broken_ssl_background_painter_.get();
    label_background = kBrokenSSLBackgroundColor;
  } else if (security_level_ == ToolbarModel::EV_SECURE) {
    painter_ = ev_background_painter_.get();
    label_background = kEVBackgroundColor;
  } else {
    painter_ = NULL;
  }

  base::string16 host =
      OriginChip::LabelFromURLForProfile(url_displayed_,
                                         toolbar_view_->browser()->profile());
  if (security_level_ == ToolbarModel::EV_SECURE) {
    host = l10n_util::GetStringFUTF16(IDS_SITE_CHIP_EV_SSL_LABEL,
        toolbar_view_->GetToolbarModel()->GetEVCertName(),
        host);
  }

  host_label_->SetText(host);
  host_label_->SetTooltipText(host);
  host_label_->SetBackgroundColor(label_background);
  host_label_->SetElideBehavior(views::Label::NO_ELIDE);

  int icon = toolbar_view_->GetToolbarModel()->GetIconForSecurityLevel(
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
        extensions::ExtensionSystem::Get(
            toolbar_view_->browser()->profile())->extension_service();
    const extensions::Extension* extension =
        service->extensions()->GetExtensionOrAppByURL(url_displayed_);
    extension_icon_.reset(
        new ToolbarOriginChipExtensionIcon(location_icon_view_,
                                           toolbar_view_->browser()->profile(),
                                           extension));
  } else {
    extension_icon_.reset();
  }

  Layout();
  SchedulePaint();
}

void ToolbarOriginChipView::OnChanged() {
  Update(toolbar_view_->GetWebContents());
  toolbar_view_->Layout();
  toolbar_view_->SchedulePaint();
  // TODO(gbillock): Also need to potentially repaint infobars to make sure the
  // arrows are pointing to the right spot. Only needed for some edge cases.
}

gfx::Size ToolbarOriginChipView::GetPreferredSize() {
  gfx::Size label_size = host_label_->GetPreferredSize();
  gfx::Size icon_size = location_icon_view_->GetPreferredSize();
  int icon_spacing = showing_16x16_icon_ ?
      (k16x16IconLeadingSpacing + k16x16IconTrailingSpacing) : 0;
  return gfx::Size(kEdgeThickness + icon_size.width() + icon_spacing +
                   kIconTextSpacing + label_size.width() +
                   kTrailingLabelMargin + kEdgeThickness,
                   icon_size.height());
}

void ToolbarOriginChipView::Layout() {
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

void ToolbarOriginChipView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect rect(GetLocalBounds());
  if (painter_)
    views::Painter::PaintPainterAt(canvas, painter_, rect);

  ToolbarButton::OnPaint(canvas);
}

int ToolbarOriginChipView::ElideDomainTarget(int target_max_width) {
  base::string16 host =
      OriginChip::LabelFromURLForProfile(url_displayed_,
                                         toolbar_view_->browser()->profile());
  host_label_->SetText(host);
  int width = GetPreferredSize().width();
  if (width <= target_max_width)
    return width;

  gfx::Size label_size = host_label_->GetPreferredSize();
  int padding_width = width - label_size.width();

  host_label_->SetText(ElideHost(
      toolbar_view_->GetToolbarModel()->GetURL(),
      host_label_->font_list(), target_max_width - padding_width));
  return GetPreferredSize().width();
}

// TODO(gbillock): Make the LocationBarView or OmniboxView the listener for
// this button.
void ToolbarOriginChipView::ButtonPressed(views::Button* sender,
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

  toolbar_view_->location_bar()->ShowURL();
}

void ToolbarOriginChipView::WriteDragDataForView(View* sender,
                                        const gfx::Point& press_pt,
                                        OSExchangeData* data) {
  // TODO(gbillock): Consolidate this with the identical logic in
  // LocationBarView.
  content::WebContents* web_contents = toolbar_view_->GetWebContents();
  FaviconTabHelper* favicon_tab_helper =
      FaviconTabHelper::FromWebContents(web_contents);
  gfx::ImageSkia favicon = favicon_tab_helper->GetFavicon().AsImageSkia();
  button_drag_utils::SetURLAndDragImage(web_contents->GetURL(),
                                        web_contents->GetTitle(),
                                        favicon,
                                        data,
                                        sender->GetWidget());
}

int ToolbarOriginChipView::GetDragOperationsForView(View* sender,
                                           const gfx::Point& p) {
  return ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK;
}

bool ToolbarOriginChipView::CanStartDragForView(View* sender,
                                       const gfx::Point& press_pt,
                                       const gfx::Point& p) {
  return true;
}

// Note: When OnSafeBrowsingHit would be called, OnSafeBrowsingMatch will
// have already been called.
void ToolbarOriginChipView::OnSafeBrowsingHit(
    const SafeBrowsingUIManager::UnsafeResource& resource) {}

void ToolbarOriginChipView::OnSafeBrowsingMatch(
    const SafeBrowsingUIManager::UnsafeResource& resource) {
  OnChanged();
}
