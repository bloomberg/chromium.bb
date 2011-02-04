// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/menu_ui.h"

#include <algorithm>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/views/native_menu_domui.h"
#include "chrome/browser/chromeos/views/webui_menu_widget.h"
#include "chrome/browser/dom_ui/web_ui_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/jstemplate_builder.h"
#include "gfx/canvas_skia.h"
#include "gfx/favicon_size.h"
#include "gfx/font.h"
#include "grit/app_resources.h"
#include "grit/browser_resources.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/accelerator.h"
#include "views/controls/menu/menu_config.h"
#include "views/controls/menu/radio_button_image_gtk.h"
#include "views/widget/widget_gtk.h"

namespace {

// a fake resource id for not loading extra resource.
const int kNoExtraResource = -1;

// A utility function that generates css font property from gfx::Font.
// NOTE: Returns UTF-8.
std::string GetFontShorthand(const gfx::Font* font) {
  std::string out;
  if (font == NULL) {
    font = &(views::MenuConfig::instance().font);
  }
  if (font->GetStyle() & gfx::Font::BOLD) {
    out.append("bold ");
  }
  if (font->GetStyle() & gfx::Font::ITALIC) {
    out.append("italic ");
  }
  if (font->GetStyle() & gfx::Font::UNDERLINED) {
    out.append("underline ");
  }

  // TODO(oshima): The font size from gfx::Font is too small when
  // used in webkit. Figure out the reason.
  out.append(base::IntToString(font->GetFontSize() + 4));
  out.append("px/");
  out.append(base::IntToString(std::max(kFavIconSize, font->GetHeight())));
  out.append("px \"");
  out.append(UTF16ToUTF8(font->GetFontName()));
  out.append("\",sans-serif");
  return out;
}

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
      web_ui_util::GetImageDataUrl(CreateMenuScrollArrowImage(true));
  static const std::string downImage =
      web_ui_util::GetImageDataUrl(CreateMenuScrollArrowImage(false));
  return up ? upImage : downImage;
}

// Returns the radio button's "on" image if |on| is true, or
// "off" image otherwise.
const std::string& GetImageDataUrlForRadio(bool on) {
  static const std::string onImage =
      web_ui_util::GetImageDataUrl(*views::GetRadioButtonImage(true));
  static const std::string offImage =
      web_ui_util::GetImageDataUrl(*views::GetRadioButtonImage(false));
  return on ? onImage : offImage;
}

/**
 * Generates a html file that uses |menu_class| as a menu implementation.
 * |menu_source_id| specifies the source that contains the definition of the
 * |menu_class|, or empty string to use plain "Menu".
 *
 * TODO(oshima): make this template to avoid repeatedly loading the
 * same source/css files.
 */
std::string GetMenuUIHTMLSourceFromString(
    const chromeos::MenuSourceDelegate* delegate,
    const base::StringPiece& menu_template,
    const std::string& menu_class,
    int menu_source_id,
    int menu_css_id) {
#define SET_INTEGER_PROPERTY(prop) \
  value_config.SetInteger(#prop, menu_config.prop)

  const views::MenuConfig& menu_config = views::MenuConfig::instance();

  DictionaryValue value_config;
  value_config.SetString("radioOnUrl", GetImageDataUrlForRadio(true));
  value_config.SetString("radioOffUrl", GetImageDataUrlForRadio(false));
  value_config.SetString(
      "arrowUrl", web_ui_util::GetImageDataUrlFromResource(IDR_MENU_ARROW));
  value_config.SetString(
      "checkUrl", web_ui_util::GetImageDataUrlFromResource(IDR_MENU_CHECK));

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

  if (delegate)
    delegate->AddCustomConfigValues(&value_config);

  std::string json_config;
  base::JSONWriter::Write(&value_config, false, &json_config);

  DictionaryValue strings;

  strings.SetString(
      "init_script",
      menu_class + ".decorate(document.getElementById('viewport'));" +
      " init(" + json_config + ");");

  if (menu_source_id == kNoExtraResource) {
    strings.SetString("menu_source", "");
  } else {
    base::StringPiece menu_source(
        ResourceBundle::GetSharedInstance().GetRawDataResource(menu_source_id));
    strings.SetString("menu_source", menu_source.as_string());
  }
  if (menu_css_id == kNoExtraResource) {
    strings.SetString("menu_css", "");
  } else {
    base::StringPiece menu_css(
        ResourceBundle::GetSharedInstance().GetRawDataResource(menu_css_id));
    strings.SetString("menu_css", menu_css.as_string());
  }
  if (delegate)
    delegate->AddLocalizedStrings(&strings);

  return jstemplate_builder::GetI18nTemplateHtml(menu_template, &strings);
}

class MenuUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  MenuUIHTMLSource(const chromeos::MenuSourceDelegate* delegate,
                   const std::string& source_name,
                   const std::string& menu_class,
                   int menu_source_id,
                   int menu_css_id);

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

  // The menu ui the source is created for.
  scoped_ptr<const chromeos::MenuSourceDelegate> delegate_;

  // The name of JS Menu class to use.
  const std::string menu_class_;

  // The resource id of the JS file of the menu subclass.
  int menu_source_id_;

  // The resource id of the CSS file of the menu subclass.
  int menu_css_id_;

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
  void HandleActivate(const ListValue* values);
  void HandleOpenSubmenu(const ListValue* values);
  void HandleCloseSubmenu(const ListValue* values);
  void HandleMoveInputToSubmenu(const ListValue* values);
  void HandleMoveInputToParent(const ListValue* values);
  void HandleCloseAll(const ListValue* values);
  void HandleModelUpdated(const ListValue* values);
  // This is a utility DOMUI message to print debug message.
  // Menu can't use dev tool as it lives outside of browser.
  // TODO(oshima): This is inconvenient and figure out how we can use
  // dev tools for menus (and other domui that does not belong to browser).
  void HandleLog(const ListValue* values);

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

MenuUIHTMLSource::MenuUIHTMLSource(const chromeos::MenuSourceDelegate* delegate,
                                   const std::string& source_name,
                                   const std::string& menu_class,
                                   int menu_source_id,
                                   int menu_css_id)
    : DataSource(source_name, MessageLoop::current()),
      delegate_(delegate),
      menu_class_(menu_class),
      menu_source_id_(menu_source_id),
      menu_css_id_(menu_css_id) {
}

void MenuUIHTMLSource::StartDataRequest(const std::string& path,
                                        bool is_off_the_record,
                                        int request_id) {
  static const base::StringPiece menu_template(
      ResourceBundle::GetSharedInstance().GetRawDataResource(IDR_MENU_HTML));

  // The resource string should be pure code and should not contain
  // i18n string.
  const std::string menu_html = GetMenuUIHTMLSourceFromString(
      delegate_.get(), menu_template, menu_class_,
      menu_source_id_, menu_css_id_);

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
      "activate",
      NewCallback(this,
                  &MenuHandler::HandleActivate));
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
  dom_ui_->RegisterMessageCallback(
      "close_all",
      NewCallback(this,
                  &MenuHandler::HandleCloseAll));
  dom_ui_->RegisterMessageCallback(
      "model_updated",
      NewCallback(this,
                  &MenuHandler::HandleModelUpdated));
  dom_ui_->RegisterMessageCallback(
      "log",
      NewCallback(this,
                  &MenuHandler::HandleLog));
}

void MenuHandler::HandleActivate(const ListValue* values) {
  CHECK_EQ(2U, values->GetSize());
  std::string index_str;
  bool success = values->GetString(0, &index_str);
  DCHECK(success);
  std::string activation;
  success = values->GetString(1, &activation);
  DCHECK(success);

  int index;
  success = base::StringToInt(index_str, &index);
  DCHECK(success);
  chromeos::WebUIMenuControl* control = GetMenuControl();
  if (control) {
    ui::MenuModel* model = GetMenuModel();
    DCHECK(model);
    DCHECK_GE(index, 0);
    DCHECK(activation == "close_and_activate" ||
           activation == "activate_no_close") << activation;
    if (model->IsEnabledAt(index)) {
      control->Activate(model,
                        index,
                        activation == "close_and_activate" ?
                        chromeos::WebUIMenuControl::CLOSE_AND_ACTIVATE :
                        chromeos::WebUIMenuControl::ACTIVATE_NO_CLOSE);
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
  chromeos::WebUIMenuControl* control = GetMenuControl();
  if (control)
    control->OpenSubmenu(index, y);
}

void MenuHandler::HandleCloseSubmenu(const ListValue* values) {
  chromeos::WebUIMenuControl* control = GetMenuControl();
  if (control)
    control->CloseSubmenu();
}

void MenuHandler::HandleMoveInputToSubmenu(const ListValue* values) {
  chromeos::WebUIMenuControl* control = GetMenuControl();
  if (control)
    control->MoveInputToSubmenu();
}

void MenuHandler::HandleMoveInputToParent(const ListValue* values) {
  chromeos::WebUIMenuControl* control = GetMenuControl();
  if (control)
    control->MoveInputToParent();
}

void MenuHandler::HandleCloseAll(const ListValue* values) {
  chromeos::WebUIMenuControl* control = GetMenuControl();
  if (control)
    control->CloseAll();
}

void MenuHandler::HandleModelUpdated(const ListValue* values) {
  ui::MenuModel* model = GetMenuModel();
  if (model)
    static_cast<chromeos::MenuUI*>(dom_ui_)->ModelUpdated(model);
}

void MenuHandler::HandleLog(const ListValue* values) {
  CHECK_EQ(1U, values->GetSize());
  std::string msg;
  bool success = values->GetString(0, &msg);
  DCHECK(success);
  DVLOG(1) << msg;
}

void MenuHandler::UpdatePreferredSize(const gfx::Size& new_size) {
  if (!loaded_)
    return;
  chromeos::WebUIMenuControl* control = GetMenuControl();
  if (control)
    control->SetSize(new_size);
}

void MenuHandler::LoadingStateChanged(TabContents* contents) {
  chromeos::WebUIMenuControl* control = GetMenuControl();
  if (control && !contents->is_loading()) {
    loaded_ = true;
    control->OnLoad();
    HandleModelUpdated(NULL);
  }
}

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
//
// MenuHandlerBase
//
////////////////////////////////////////////////////////////////////////////////

chromeos::WebUIMenuControl* MenuHandlerBase::GetMenuControl() {
  WebUIMenuWidget* widget =
      chromeos::WebUIMenuWidget::FindWebUIMenuWidget(
          dom_ui_->tab_contents()->GetNativeView());
  if (widget)
    return widget->domui_menu();  // NativeMenuDOMUI implements WebUIMenuControl
  else
    return NULL;
}

ui::MenuModel* MenuHandlerBase::GetMenuModel() {
  WebUIMenuControl* control = GetMenuControl();
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

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          ChromeURLDataManager::GetInstance(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(CreateDataSource())));
}

MenuUI::MenuUI(TabContents* contents, ChromeURLDataManager::DataSource* source)
    : DOMUI(contents) {
  MenuHandler* handler = new MenuHandler();
  AddMessageHandler((handler)->Attach(this));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          ChromeURLDataManager::GetInstance(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(source)));
}

void MenuUI::ModelUpdated(const ui::MenuModel* model) {
  DictionaryValue json_model;
  ListValue* items = new ListValue();
  json_model.Set("items", items);
  int max_icon_width = 0;
  bool has_accelerator = false;
  for (int index = 0; index < model->GetItemCount(); ++index) {
    ui::MenuModel::ItemType type = model->GetTypeAt(index);
    DictionaryValue* item;
    switch (type) {
      case ui::MenuModel::TYPE_SEPARATOR:
        item = CreateMenuItem(model, index, "separator",
                              &max_icon_width, &has_accelerator);
        break;
      case ui::MenuModel::TYPE_RADIO:
        max_icon_width = std::max(max_icon_width, 12);
        item = CreateMenuItem(model, index, "radio",
                              &max_icon_width, &has_accelerator);
        break;
      case ui::MenuModel::TYPE_SUBMENU:
        item = CreateMenuItem(model, index, "submenu",
                              &max_icon_width, &has_accelerator);
        break;
      case ui::MenuModel::TYPE_COMMAND:
        item = CreateMenuItem(model, index, "command",
                              &max_icon_width, &has_accelerator);
        break;
      case ui::MenuModel::TYPE_CHECK:
        // Add space even when unchecked.
        max_icon_width = std::max(max_icon_width, 12);
        item = CreateMenuItem(model, index, "check",
                              &max_icon_width, &has_accelerator);
        break;
      default:
        // TODO(oshima): We don't support BUTTOM_ITEM for now.
        NOTREACHED();
        continue;
    }
    items->Set(index, item);
  }
  WebUIMenuWidget* widget =
      chromeos::WebUIMenuWidget::FindWebUIMenuWidget(
          tab_contents()->GetNativeView());
  DCHECK(widget);
  json_model.SetInteger("maxIconWidth", max_icon_width);
  json_model.SetBoolean("isRoot", widget->is_root());
  json_model.SetBoolean("hasAccelerator", has_accelerator);
  CallJavascriptFunction(L"updateModel", json_model);
}

DictionaryValue* MenuUI::CreateMenuItem(const ui::MenuModel* model,
                                        int index,
                                        const char* type,
                                        int* max_icon_width,
                                        bool* has_accel) const {
  // Note: Web UI uses '&' as mnemonic.
  string16 label16 = model->GetLabelAt(index);
  DictionaryValue* item = new DictionaryValue();

  item->SetString("type", type);
  item->SetString("label", label16);
  item->SetBoolean("enabled", model->IsEnabledAt(index));
  item->SetBoolean("visible", model->IsVisibleAt(index));
  item->SetBoolean("checked", model->IsItemCheckedAt(index));
  item->SetInteger("command_id", model->GetCommandIdAt(index));
  item->SetString(
      "font", GetFontShorthand(model->GetLabelFontAt(index)));
  SkBitmap icon;
  if (model->GetIconAt(index, &icon) && !icon.isNull() && !icon.empty()) {
    item->SetString("icon", web_ui_util::GetImageDataUrl(icon));
    *max_icon_width = std::max(*max_icon_width, icon.width());
  }
  views::Accelerator menu_accelerator;
  if (model->GetAcceleratorAt(index, &menu_accelerator)) {
    item->SetString("accel", menu_accelerator.GetShortcutText());
    *has_accel = true;
  }
  return item;
}

// static
ChromeURLDataManager::DataSource* MenuUI::CreateMenuUIHTMLSource(
    const MenuSourceDelegate* delegate,
    const std::string& source_name,
    const std::string& menu_class,
    int menu_source_id,
    int menu_css_id) {
  return new MenuUIHTMLSource(delegate,
                              source_name,
                              menu_class,
                              menu_source_id,
                              menu_css_id);
}

// static
bool MenuUI::IsEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDOMUIMenu);
}

ChromeURLDataManager::DataSource* MenuUI::CreateDataSource() {
  return CreateMenuUIHTMLSource(NULL,
                                chrome::kChromeUIMenu,
                                "Menu" /* class name */,
                                kNoExtraResource,
                                kNoExtraResource);
}

}  // namespace chromeos
