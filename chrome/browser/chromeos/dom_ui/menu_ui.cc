// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/menu_ui.h"

#include "app/menus/menu_model.h"
#include "app/resource_bundle.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/views/domui_menu_widget.h"
#include "chrome/browser/chromeos/views/native_menu_domui.h"
#include "chrome/browser/dom_ui/dom_ui_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_fetcher.h"
#include "gfx/canvas_skia.h"
#include "grit/app_resources.h"
#include "grit/browser_resources.h"
#include "views/controls/menu/menu_config.h"
#include "views/controls/menu/radio_button_image_gtk.h"
#include "views/widget/widget_gtk.h"

namespace {

// Creates scroll button's up image when |up| is true or
// down image if |up| is false.
SkBitmap CreateMenuScrollArrowImage(bool up) {
  const views::MenuConfig& config = views::MenuConfig::instance();

  int height = config.scroll_arrow_height;
  gfx::CanvasSkia canvas(height * 2, height, false);

  if (!up) {
    // flip the direction.
    canvas.scale(1.0, -1.0);
    canvas.translate(0, -height);
  }
  // Draw triangle.
  SkPath path;
  path.moveTo(height, 0);
  path.lineTo(0, height);
  path.lineTo(height * 2, height);
  SkPaint paint;
  paint.setColor(SK_ColorBLACK);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);
  canvas.drawPath(path, paint);
  return canvas.ExtractBitmap();
}

// Returns the scroll button's up image if |up| is true, or
// "down" image otherwise.
const std::string& GetImageDataUrlForMenuScrollArrow(bool up) {
  static const std::string upImage =
      dom_ui_util::GetImageDataUrl(CreateMenuScrollArrowImage(true));
  static const std::string downImage =
      dom_ui_util::GetImageDataUrl(CreateMenuScrollArrowImage(false));
  return up ? upImage : downImage;
}

// Returns the radio button's "on" image if |on| is true, or
// "off" image otherwise.
const std::string& GetImageDataUrlForRadio(bool on) {
  static const std::string onImage =
      dom_ui_util::GetImageDataUrl(*views::GetRadioButtonImage(true));
  static const std::string offImage =
       dom_ui_util::GetImageDataUrl(*views::GetRadioButtonImage(false));
  return on ? onImage : offImage;
}

std::string GetMenuUIHTMLSourceFromString(const std::string& menu_html) {
#define SET_INTEGER_PROPERTY(prop) \
  value_config.SetInteger(#prop, menu_config.prop)

  const views::MenuConfig& menu_config = views::MenuConfig::instance();

  DictionaryValue value_config;
  value_config.SetString("radioOnUrl", GetImageDataUrlForRadio(true));
  value_config.SetString("radioOffUrl", GetImageDataUrlForRadio(false));
  value_config.SetString(
      "arrowUrl", dom_ui_util::GetImageDataUrlFromResource(IDR_MENU_ARROW));
  value_config.SetString(
      "checkUrl", dom_ui_util::GetImageDataUrlFromResource(IDR_MENU_CHECK));

  value_config.SetString(
      "scrollUpUrl", GetImageDataUrlForMenuScrollArrow(true));
  value_config.SetString(
      "scrollDownUrl", GetImageDataUrlForMenuScrollArrow(false));

  SET_INTEGER_PROPERTY(item_top_margin);
  SET_INTEGER_PROPERTY(item_bottom_margin);
  SET_INTEGER_PROPERTY(item_no_icon_top_margin);
  SET_INTEGER_PROPERTY(item_no_icon_bottom_margin);
  SET_INTEGER_PROPERTY(item_left_margin);
  SET_INTEGER_PROPERTY(label_to_arrow_padding);
  SET_INTEGER_PROPERTY(arrow_to_edge_padding);
  SET_INTEGER_PROPERTY(icon_to_label_padding);
  SET_INTEGER_PROPERTY(gutter_to_label);
  SET_INTEGER_PROPERTY(check_width);
  SET_INTEGER_PROPERTY(check_height);
  SET_INTEGER_PROPERTY(radio_width);
  SET_INTEGER_PROPERTY(radio_height);
  SET_INTEGER_PROPERTY(arrow_height);
  SET_INTEGER_PROPERTY(arrow_width);
  SET_INTEGER_PROPERTY(gutter_width);
  SET_INTEGER_PROPERTY(separator_height);
  SET_INTEGER_PROPERTY(render_gutter);
  SET_INTEGER_PROPERTY(show_mnemonics);
  SET_INTEGER_PROPERTY(scroll_arrow_height);
  SET_INTEGER_PROPERTY(label_to_accelerator_padding);

  std::string json_config;
  base::JSONWriter::Write(&value_config, false, &json_config);
  return menu_html + "<script>init(" + json_config + ");</script>";
}

// Returns the menu's html code given by the resource id with the code
// to intialization the menu. The resource string should be pure code
// and should not contain i18n string.
std::string GetMenuUIHTMLSourceFromResource(int res) {
  const base::StringPiece menu_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(res));
  return GetMenuUIHTMLSourceFromString(menu_html.as_string());
}

class MenuUIHTMLSource : public ChromeURLDataManager::DataSource,
                         public URLFetcher::Delegate {
 public:
  explicit MenuUIHTMLSource(Profile* profile);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

  // URLFetcher::Delegate implements:
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

 private:
  virtual ~MenuUIHTMLSource() {}
#ifndef NDEBUG
  int request_id_;
  Profile* profile_;
#endif
  DISALLOW_COPY_AND_ASSIGN(MenuUIHTMLSource);
};

// The handler for Javascript messages related to the "system" view.
class MenuHandler : public chromeos::MenuHandlerBase,
                    public base::SupportsWeakPtr<MenuHandler>,
                    public TabContentsDelegate {
 public:
  MenuHandler();
  virtual ~MenuHandler();

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

 private:
  void HandleClick(const ListValue* values);
  void HandleOpenSubmenu(const ListValue* values);
  void HandleCloseSubmenu(const ListValue* values);
  void HandleMoveInputToSubmenu(const ListValue* values);
  void HandleMoveInputToParent(const ListValue* values);

  // TabContentsDelegate implements:
  virtual void UpdatePreferredSize(const gfx::Size& new_size);
  virtual void LoadingStateChanged(TabContents* contents);

  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition) {}
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {}
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) {}

  // TODO(oshima): Handle crash
  virtual void ActivateContents(TabContents* contents) {}
  virtual void DeactivateContents(TabContents* contents) {}
  virtual void CloseContents(TabContents* source) {}
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  virtual bool IsPopup(const TabContents* source) { return false; }
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}
  virtual void URLStarredChanged(TabContents* source, bool starred) {}
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}
  virtual bool CanDownload(int request_id) { return false; }
  virtual bool infobars_enabled() { return false; }
  virtual bool ShouldEnablePreferredSizeNotifications() { return true; }
  virtual bool CanReloadContents(TabContents* source) const { return false; }

  // True if the page is loaded.
  bool loaded_;

  DISALLOW_COPY_AND_ASSIGN(MenuHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// MenuUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

MenuUIHTMLSource::MenuUIHTMLSource(Profile* profile)
    : DataSource(chrome::kChromeUIMenu, MessageLoop::current())
#ifndef NDEBUG
    , request_id_(-1),
      profile_(profile)
#endif
     {
}

void MenuUIHTMLSource::StartDataRequest(const std::string& path,
                                        bool is_off_the_record,
                                        int request_id) {
#ifndef NDEBUG
  std::string url = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kDOMUIMenuUrl);
  if (!url.empty()) {
    request_id_ = request_id;
    URLFetcher* fetcher = new URLFetcher(GURL(url), URLFetcher::GET, this);
    fetcher->set_request_context(profile_->GetRequestContext());
    fetcher->Start();
    return;
  }
#endif

  static const std::string menu_html =
      GetMenuUIHTMLSourceFromResource(IDR_MENU_HTML);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);

  // TODO(oshima): Eliminate boilerplate code. See http://crbug.com/57583 .
  html_bytes->data.resize(menu_html.size());
  std::copy(menu_html.begin(), menu_html.end(),
            html_bytes->data.begin());
  SendResponse(request_id, html_bytes);
}

void MenuUIHTMLSource::OnURLFetchComplete(const URLFetcher* source,
                                          const GURL& url,
                                          const URLRequestStatus& status,
                                          int response_code,
                                          const ResponseCookies& cookies,
                                          const std::string& data) {
  const std::string menu_html =
      GetMenuUIHTMLSourceFromString(data);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);

  // TODO(oshima): Eliminate boilerplate code. See http://crbug.com/57583 .
  html_bytes->data.resize(menu_html.size());
  std::copy(menu_html.begin(), menu_html.end(),
            html_bytes->data.begin());
  SendResponse(request_id_, html_bytes);

  delete source;
}


////////////////////////////////////////////////////////////////////////////////
//
// MenuHandler
//
////////////////////////////////////////////////////////////////////////////////
MenuHandler::MenuHandler() : loaded_(false) {
}

MenuHandler::~MenuHandler() {
}

DOMMessageHandler* MenuHandler::Attach(DOMUI* dom_ui) {
  DOMMessageHandler* handler = DOMMessageHandler::Attach(dom_ui);
  dom_ui->tab_contents()->set_delegate(this);
  return handler;
}

void MenuHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback(
      "click",
      NewCallback(this,
                  &MenuHandler::HandleClick));
  dom_ui_->RegisterMessageCallback(
      "open_submenu",
      NewCallback(this,
                  &MenuHandler::HandleOpenSubmenu));
  dom_ui_->RegisterMessageCallback(
      "close_submenu",
      NewCallback(this,
                  &MenuHandler::HandleCloseSubmenu));
  dom_ui_->RegisterMessageCallback(
      "move_to_submenu",
      NewCallback(this,
                  &MenuHandler::HandleMoveInputToSubmenu));
  dom_ui_->RegisterMessageCallback(
      "move_to_parent",
      NewCallback(this,
                  &MenuHandler::HandleMoveInputToParent));
}

void MenuHandler::HandleClick(const ListValue* values) {
  CHECK_EQ(1U, values->GetSize());
  std::string index_str;
  bool success = values->GetString(0, &index_str);
  DCHECK(success);
  int index;
  success = base::StringToInt(index_str, &index);
  DCHECK(success);
  chromeos::DOMUIMenuControl* control = GetMenuControl();
  if (control) {
    menus::MenuModel* model = GetMenuModel();
    DCHECK(model);
    if (index < 0) {
      control->CloseAll();
    } else if (model->IsEnabledAt(index)) {
      control->Activate(model, index);
    }
  }
}

void MenuHandler::HandleOpenSubmenu(const ListValue* values) {
  CHECK_EQ(2U, values->GetSize());
  std::string index_str;
  bool success = values->GetString(0, &index_str);
  DCHECK(success);
  std::string y_str;
  success = values->GetString(1, &y_str);
  DCHECK(success);
  int index;
  success = base::StringToInt(index_str, &index);
  DCHECK(success);
  int y;
  success = base::StringToInt(y_str, &y);
  DCHECK(success);
  chromeos::DOMUIMenuControl* control = GetMenuControl();
  if (control)
    control->OpenSubmenu(index, y);
}

void MenuHandler::HandleCloseSubmenu(const ListValue* values) {
  chromeos::DOMUIMenuControl* control = GetMenuControl();
  if (control)
    control->CloseSubmenu();
}

void MenuHandler::HandleMoveInputToSubmenu(const ListValue* values) {
  chromeos::DOMUIMenuControl* control = GetMenuControl();
  if (control)
    control->MoveInputToSubmenu();
}

void MenuHandler::HandleMoveInputToParent(const ListValue* values) {
  chromeos::DOMUIMenuControl* control = GetMenuControl();
  if (control)
    control->MoveInputToParent();
}

void MenuHandler::UpdatePreferredSize(const gfx::Size& new_size) {
  if (!loaded_)
    return;
  chromeos::DOMUIMenuControl* control = GetMenuControl();
  if (control)
    control->SetSize(new_size);
}

void MenuHandler::LoadingStateChanged(TabContents* contents) {
  chromeos::DOMUIMenuControl* control = GetMenuControl();
  if (control && !contents->is_loading()) {
    loaded_ = true;
    control->OnLoad();
  }
}

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
//
// MenuHandlerBase
//
////////////////////////////////////////////////////////////////////////////////

chromeos::DOMUIMenuControl* MenuHandlerBase::GetMenuControl() {
  DOMUIMenuWidget* widget =
      chromeos::DOMUIMenuWidget::FindDOMUIMenuWidget(
          dom_ui_->tab_contents()->GetNativeView());
  if (widget)
    return widget->domui_menu();  // NativeMenuDOMUI implements DOMUIMenuControl
  else
    return NULL;
}

menus::MenuModel* MenuHandlerBase::GetMenuModel() {
  DOMUIMenuControl* control = GetMenuControl();
  if (control)
    return control->GetMenuModel();
  else
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuUI
//
////////////////////////////////////////////////////////////////////////////////

MenuUI::MenuUI(TabContents* contents) : DOMUI(contents) {
  MenuHandler* handler = new MenuHandler();
  AddMessageHandler((handler)->Attach(this));

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(CreateDataSource())));
}

ChromeURLDataManager::DataSource* MenuUI::CreateDataSource() {
  return new MenuUIHTMLSource(GetProfile());
}

}  // namespace chromeos
