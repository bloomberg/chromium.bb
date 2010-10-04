// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/menu_ui.h"

#include "app/menus/menu_model.h"
#include "app/resource_bundle.h"
#include "base/callback.h"
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
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/url_constants.h"
#include "grit/app_resources.h"
#include "grit/browser_resources.h"
#include "views/controls/menu/menu_config.h"
#include "views/controls/menu/radio_button_image_gtk.h"
#include "views/widget/widget_gtk.h"

namespace {

std::string GetImageUrlForRadioOn() {
  return dom_ui_util::GetImageDataUrl(*views::GetRadioButtonImage(true));
}

std::string GetImageUrlForRadioOff() {
  return dom_ui_util::GetImageDataUrl(*views::GetRadioButtonImage(false));
}

class MenuUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  MenuUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  virtual ~MenuUIHTMLSource() {}

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

MenuUIHTMLSource::MenuUIHTMLSource()
    : DataSource(chrome::kChromeUIMenu, MessageLoop::current()) {
}

void MenuUIHTMLSource::StartDataRequest(const std::string& path,
                                        bool is_off_the_record,
                                        int request_id) {
  static const std::string menu_html =
      chromeos::GetMenuUIHTMLSourceFromResource(IDR_MENU_HTML);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);

  // TODO(oshima): Eliminate boilerplate code. See http://crbug.com/57583 .
  html_bytes->data.resize(menu_html.size());
  std::copy(menu_html.begin(), menu_html.end(),
            html_bytes->data.begin());
  SendResponse(request_id, html_bytes);
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
  std::string value;
  bool success = values->GetString(0, &value);
  DCHECK(success);

  int index;
  success = base::StringToInt(value, &index);
  DCHECK(success) << " Faild to convert string to int " << value;

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
  std::string index_str;
  values->GetString(0, &index_str);
  std::string y_str;
  values->GetString(1, &y_str);
  int index;
  int y;
  base::StringToInt(index_str, &index);
  base::StringToInt(y_str, &y);
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
  return new MenuUIHTMLSource();
}

std::string GetMenuUIHTMLSourceFromResource(int res) {
#define SET_INTEGER_PROPERTY(prop) \
  value_config.SetInteger(#prop, menu_config.prop)

  const base::StringPiece menu_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(res));

  const views::MenuConfig& menu_config = views::MenuConfig::instance();

  DictionaryValue value_config;
  value_config.SetString("radioOnUrl", GetImageUrlForRadioOn());
  value_config.SetString("radioOffUrl", GetImageUrlForRadioOff());
  value_config.SetString(
      "arrowUrl", dom_ui_util::GetImageDataUrlFromResource(IDR_MENU_ARROW));
  value_config.SetString(
      "checkUrl", dom_ui_util::GetImageDataUrlFromResource(IDR_MENU_CHECK));

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

  return menu_html.as_string() +
      "<script>init(" + json_config + ");</script>";
}

}  // namespace chromeos
