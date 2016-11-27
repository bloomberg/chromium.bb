// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/model_view_channel.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_screen.h"
#include "components/login/base_screen_handler_utils.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace login {
class LocalizedValuesBuilder;
}

namespace chromeos {

class BaseScreen;
class OobeUI;

// Base class for the OOBE/Login WebUI handlers.
class BaseScreenHandler : public content::WebUIMessageHandler,
                          public ModelViewChannel {
 public:
  // C-tor used when JS screen prefix is not needed.
  BaseScreenHandler();

  // C-tor used when JS screen prefix is needed.
  explicit BaseScreenHandler(const std::string& js_screen_path);

  ~BaseScreenHandler() override;

  // Gets localized strings to be used on the page.
  void GetLocalizedStrings(
      base::DictionaryValue* localized_strings);

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // ModelViewChannel implementation:
  void CommitContextChanges(const base::DictionaryValue& diff) override;

  // This method is called when page is ready. It propagates to inherited class
  // via virtual Initialize() method (see below).
  void InitializeBase();

  void set_async_assets_load_id(const std::string& async_assets_load_id) {
    async_assets_load_id_ = async_assets_load_id;
  }
  const std::string& async_assets_load_id() const {
    return async_assets_load_id_;
  }

 protected:
  // All subclasses should implement this method to provide localized values.
  virtual void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) = 0;

  // All subclasses should implement this method to register callbacks for JS
  // messages.
  //
  // TODO (ygorshenin, crbug.com/433797): make this method purely vrtual when
  // all screens will be switched to use ScreenContext.
  virtual void DeclareJSCallbacks() {}

  // Subclasses can override these methods to pass additional parameters
  // to loadTimeData. Generally, it is a bad approach, and it should be replaced
  // with Context at some point.
  virtual void GetAdditionalParameters(base::DictionaryValue* parameters);

  // Shortcut for calling JS methods on WebUI side.
  void CallJS(const std::string& method);

  template<typename A1>
  void CallJS(const std::string& method, const A1& arg1) {
    web_ui()->CallJavascriptFunctionUnsafe(FullMethodPath(method),
                                           ::login::MakeValue(arg1));
  }

  template<typename A1, typename A2>
  void CallJS(const std::string& method, const A1& arg1, const A2& arg2) {
    web_ui()->CallJavascriptFunctionUnsafe(FullMethodPath(method),
                                           ::login::MakeValue(arg1),
                                           ::login::MakeValue(arg2));
  }

  template<typename A1, typename A2, typename A3>
  void CallJS(const std::string& method,
              const A1& arg1,
              const A2& arg2,
              const A3& arg3) {
    web_ui()->CallJavascriptFunctionUnsafe(
        FullMethodPath(method), ::login::MakeValue(arg1),
        ::login::MakeValue(arg2), ::login::MakeValue(arg3));
  }

  template<typename A1, typename A2, typename A3, typename A4>
  void CallJS(const std::string& method,
              const A1& arg1,
              const A2& arg2,
              const A3& arg3,
              const A4& arg4) {
    web_ui()->CallJavascriptFunctionUnsafe(
        FullMethodPath(method), ::login::MakeValue(arg1),
        ::login::MakeValue(arg2), ::login::MakeValue(arg3),
        ::login::MakeValue(arg4));
  }

  // Shortcut methods for adding WebUI callbacks.
  template<typename T>
  void AddRawCallback(const std::string& name,
                      void (T::*method)(const base::ListValue* args)) {
    web_ui()->RegisterMessageCallback(
        name,
        base::Bind(method, base::Unretained(static_cast<T*>(this))));
  }

  template<typename T, typename... Args>
  void AddCallback(const std::string& name, void (T::*method)(Args...)) {
    base::Callback<void(Args...)> callback =
        base::Bind(method, base::Unretained(static_cast<T*>(this)));
    web_ui()->RegisterMessageCallback(
        name, base::Bind(&::login::CallbackWrapper<Args...>, callback));
  }

  template <typename Method>
  void AddPrefixedCallback(const std::string& unprefixed_name,
                           const Method& method) {
    AddCallback(FullMethodPath(unprefixed_name), method);
  }

  // Called when the page is ready and handler can do initialization.
  virtual void Initialize() = 0;

  // Show selected WebUI |screen|.
  void ShowScreen(OobeScreen screen);
  // Show selected WebUI |screen|. Pass screen initialization using the |data|
  // parameter.
  void ShowScreenWithData(OobeScreen screen, const base::DictionaryValue* data);

  // Returns the OobeUI instance.
  OobeUI* GetOobeUI() const;

  // Returns current visible OOBE screen.
  OobeScreen GetCurrentScreen() const;

  // Whether page is ready.
  bool page_is_ready() const { return page_is_ready_; }

  // Returns the window which shows us.
  virtual gfx::NativeWindow GetNativeWindow();

  void SetBaseScreen(BaseScreen* base_screen);

 private:
  // Returns full name of JS method based on screen and method
  // names.
  std::string FullMethodPath(const std::string& method) const;

  // Handles user action.
  void HandleUserAction(const std::string& action_id);

  // Handles situation when screen context is changed.
  void HandleContextChanged(const base::DictionaryValue* diff);

  // Keeps whether page is ready.
  bool page_is_ready_;

  BaseScreen* base_screen_;

  // Full name of the corresponding JS screen object. Can be empty, if
  // there are no corresponding screen object or several different
  // objects.
  std::string js_screen_path_prefix_;

  // The string id used in the async asset load in JS. If it is set to a
  // non empty value, the Initialize will be deferred until the underlying load
  // is finished.
  std::string async_assets_load_id_;

  // Pending changes to context which will be sent when the page will be ready.
  base::DictionaryValue pending_context_changes_;

  DISALLOW_COPY_AND_ASSIGN(BaseScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_
