// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/site_chip_view.h"

#include "base/files/file_path.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_icon_image.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/views/background.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/painter.h"


// SiteChipExtensionIcon ------------------------------------------------------

class SiteChipExtensionIcon : public extensions::IconImage::Observer {
 public:
  SiteChipExtensionIcon(LocationIconView* icon_view,
                        Profile* profile,
                        const extensions::Extension* extension);
  virtual ~SiteChipExtensionIcon();

  // IconImage::Observer:
  virtual void OnExtensionIconImageChanged(
      extensions::IconImage* image) OVERRIDE;

 private:
  LocationIconView* icon_view_;
  scoped_ptr<extensions::IconImage> icon_image_;

  DISALLOW_COPY_AND_ASSIGN(SiteChipExtensionIcon);
};

SiteChipExtensionIcon::SiteChipExtensionIcon(
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

SiteChipExtensionIcon::~SiteChipExtensionIcon() {
}

void SiteChipExtensionIcon::OnExtensionIconImageChanged(
    extensions::IconImage* image) {
  if (icon_view_)
    icon_view_->SetImage(&icon_image_->image_skia());
}


// SiteChipView ---------------------------------------------------------------

namespace {

const int kEdgeThickness = 5;
const int k16x16IconLeadingSpacing = 3;
const int k16x16IconTrailingSpacing = 3;
const int kIconTextSpacing = 3;
const int kTrailingLabelMargin = 0;

}  // namespace

string16 SiteChipView::SiteLabelFromURL(const GURL& url) {
  // The NTP.
  if (!url.is_valid())
    return string16(UTF8ToUTF16("Chrome"));

  // TODO(gbillock): for kChromeUIScheme and kAboutScheme, return the title of
  // the page.
  // See url_constants.cc for hosts. ?? Or just show "Chrome"?
  if (url.SchemeIs(chrome::kChromeUIScheme) ||
      url.SchemeIs(chrome::kAboutScheme)) {
    return string16(UTF8ToUTF16("Chrome"));
  }

  // For file: urls, return the full URL.
  if (url.SchemeIsFile())
    return base::UTF8ToUTF16(url.spec());

  // TODO(gbillock): Handle filesystem urls the same way?
  // Also: should handle interstitials differently?

  // TODO(gbillock): think about view-source?

  Profile* profile = toolbar_view_->browser()->profile();

  // For chrome-extension urls, return the extension name.
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    ExtensionService* service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    const extensions::Extension* extension =
        service->extensions()->GetExtensionOrAppByURL(url);
    return extension ?
        base::UTF8ToUTF16(extension->name()) : UTF8ToUTF16(url.host());
  }

  if (url.SchemeIsHTTPOrHTTPS()) {
    // See ToolbarModelImpl::GetText(). Does not pay attention to any user
    // edits, and uses GetURL/net::FormatUrl -- We don't really care about
    // length or the autocomplete parser.
    // TODO(gbillock): This uses an algorithm very similar to GetText, which
    // is probably too conservative. Try out just using a simpler mechanism of
    // StripWWW() and IDNToUnicode().
    std::string languages;
    if (profile)
      languages = profile->GetPrefs()->GetString(prefs::kAcceptLanguages);

    base::string16 formatted = net::FormatUrl(url.GetOrigin(), languages,
        net::kFormatUrlOmitAll, net::UnescapeRule::NORMAL, NULL, NULL, NULL);
    // Remove scheme, "www.", and trailing "/".
    if (StartsWith(formatted, ASCIIToUTF16("http://"), false))
      formatted = formatted.substr(7);
    if (StartsWith(formatted, ASCIIToUTF16("https://"), false))
      formatted = formatted.substr(8);
    if (StartsWith(formatted, ASCIIToUTF16("www."), false))
      formatted = formatted.substr(4);
    if (EndsWith(formatted, ASCIIToUTF16("/"), false))
      formatted = formatted.substr(0, formatted.size()-1);
    return formatted;
  }

  // For FTP, prepend "ftp:" to hostname.
  if (url.SchemeIs(content::kFtpScheme)) {
    return string16(UTF8ToUTF16(std::string("ftp:") + url.host()));
  }

  // If all else fails, return hostname.
  return string16(UTF8ToUTF16(url.host()));
}

SiteChipView::SiteChipView(ToolbarView* toolbar_view)
    : ToolbarButton(this, NULL),
      toolbar_view_(toolbar_view),
      painter_(NULL),
      showing_16x16_icon_(false) {
  set_drag_controller(this);
}

SiteChipView::~SiteChipView() {
}

void SiteChipView::Init() {
  ToolbarButton::Init();

  // TODO(gbillock): Would be nice to just use stock LabelButton stuff here.
  location_icon_view_ = new LocationIconView(toolbar_view_->location_bar());
  host_label_ = new views::Label();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  host_label_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));

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
}

bool SiteChipView::ShouldShow() {
  return chrome::ShouldDisplayOriginChip();
}

void SiteChipView::Update(content::WebContents* web_contents) {
  if (!web_contents)
    return;

  // Note: security level can change async as the connection is made.
  GURL url = toolbar_view_->GetToolbarModel()->GetURL();
  const ToolbarModel::SecurityLevel security_level =
      toolbar_view_->GetToolbarModel()->GetSecurityLevel(true);
  if ((url == url_displayed_) && (security_level == security_level_))
    return;

  url_displayed_ = url;

  string16 host = SiteLabelFromURL(url);

  // TODO(gbillock): Deal with RTL here better? Use a separate
  // label for cert name?
  if ((security_level != security_level_) &&
      (security_level == ToolbarModel::EV_SECURE)) {
    host = l10n_util::GetStringFUTF16(IDS_SITE_CHIP_EV_SSL_LABEL,
        toolbar_view_->GetToolbarModel()->GetEVCertName(),
        host);
  }

  host_ = host;
  host_label_->SetText(host_);
  host_label_->SetTooltipText(host_);
  SkColor toolbar_background =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR);
  host_label_->SetEnabledColor(
      color_utils::GetReadableColor(SK_ColorBLACK, toolbar_background));

  security_level_ = security_level;
  int icon = toolbar_view_->GetToolbarModel()->GetIconForSecurityLevel(
      security_level_);
  showing_16x16_icon_ = false;

  if (!url.is_valid() ||
      url.SchemeIs(chrome::kChromeUIScheme)) {
    icon = IDR_PRODUCT_LOGO_16;
    showing_16x16_icon_ = true;
  }

  if (url.SchemeIs(extensions::kExtensionScheme)) {
    icon = IDR_EXTENSIONS_FAVICON;
    showing_16x16_icon_ = true;

    ExtensionService* service =
        extensions::ExtensionSystem::Get(
            toolbar_view_->browser()->profile())->extension_service();
    const extensions::Extension* extension =
        service->extensions()->GetExtensionOrAppByURL(url);
    extension_icon_.reset(
        new SiteChipExtensionIcon(location_icon_view_,
                                  toolbar_view_->browser()->profile(),
                                  extension));
  } else {
    extension_icon_.reset();
  }

  location_icon_view_->SetImage(GetThemeProvider()->GetImageSkiaNamed(icon));
  location_icon_view_->ShowTooltip(true);

  // TODO(gbillock): Add malware accounting.
  if (security_level_ == ToolbarModel::SECURITY_ERROR)
    painter_ = broken_ssl_background_painter_.get();
  else if (security_level_ == ToolbarModel::EV_SECURE)
    painter_ = ev_background_painter_.get();
  else
    painter_ = NULL;

  Layout();
  SchedulePaint();
  // TODO(gbillock): Need to schedule paint on parent to erase any background?
}

void SiteChipView::OnChanged() {
  Update(toolbar_view_->GetWebContents());
  toolbar_view_->Layout();
  toolbar_view_->SchedulePaint();
  // TODO(gbillock): Also need to potentially repaint infobars to make sure the
  // arrows are pointing to the right spot. Only needed for some edge cases.
}

gfx::Size SiteChipView::GetPreferredSize() {
  gfx::Size label_size = host_label_->GetPreferredSize();
  gfx::Size icon_size = location_icon_view_->GetPreferredSize();
  int icon_spacing = showing_16x16_icon_ ?
      (k16x16IconLeadingSpacing + k16x16IconTrailingSpacing) : 0;
  return gfx::Size(kEdgeThickness + icon_size.width() + icon_spacing +
                   kIconTextSpacing + label_size.width() +
                   kTrailingLabelMargin + kEdgeThickness,
                   icon_size.height());
}

void SiteChipView::Layout() {
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

void SiteChipView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect rect(GetLocalBounds());
  if (painter_)
    views::Painter::PaintPainterAt(canvas, painter_, rect);

  ToolbarButton::OnPaint(canvas);
}

// TODO(gbillock): Make the LocationBarView or OmniboxView the listener for
// this button.
void SiteChipView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  toolbar_view_->location_bar()->GetOmniboxView()->SetFocus();
  toolbar_view_->location_bar()->GetOmniboxView()->SelectAll(true);
  toolbar_view_->location_bar()->GetOmniboxView()->model()->
      SetCaretVisibility(true);
}

void SiteChipView::WriteDragDataForView(View* sender,
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

int SiteChipView::GetDragOperationsForView(View* sender,
                                           const gfx::Point& p) {
  return ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK;
}

bool SiteChipView::CanStartDragForView(View* sender,
                                       const gfx::Point& press_pt,
                                       const gfx::Point& p) {
  return true;
}

