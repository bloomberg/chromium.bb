// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler_utils.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace chromeos {

// Class that collects Localized Values for translation.
class LocalizedValuesBuilder {
 public:
  explicit LocalizedValuesBuilder(base::DictionaryValue* dict);
  // Method to declare localized value. |key| is the i18n key used in html.
  // |message| is text of the message.
  void Add(const std::string& key, const std::string& message);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message| is text of the message.
  void Add(const std::string& key, const base::string16& message);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message.
  void Add(const std::string& key, int message_id);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // one format parameter subsituted by |a|.
  void AddF(const std::string& key,
            int message_id,
            const base::string16& a);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // two format parameters subsituted by |a| and |b| respectively.
  void AddF(const std::string& key,
            int message_id,
            const base::string16& a,
            const base::string16& b);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // one format parameter subsituted by resource identified by |message_id_a|.
  void AddF(const std::string& key,
            int message_id,
            int message_id_a);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // two format parameters subsituted by resource identified by |message_id_a|
  // and |message_id_b| respectively.
  void AddF(const std::string& key,
            int message_id,
            int message_id_a,
            int message_id_b);
 private:
  // Not owned.
  base::DictionaryValue* dict_;
};

// Base class for the OOBE/Login WebUI handlers.
class BaseScreenHandler : public content::WebUIMessageHandler {
 public:
  // C-tor used when JS screen prefix is not needed.
  BaseScreenHandler();

  // C-tor used when JS screen prefix is needed.
  explicit BaseScreenHandler(const std::string& js_screen_path);

  virtual ~BaseScreenHandler();

  // Gets localized strings to be used on the page.
  void GetLocalizedStrings(
      base::DictionaryValue* localized_strings);

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
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) = 0;

  // Subclasses can override these methods to pass additional parameters
  // to loadTimeData. Generally, it is a bad approach, and it should be replaced
  // with Context at some point.
  virtual void GetAdditionalParameters(base::DictionaryValue* parameters);

  // Shortcut for calling JS methods on WebUI side.
  void CallJS(const std::string& method);

  template<typename A1>
  void CallJS(const std::string& method, const A1& arg1) {
    web_ui()->CallJavascriptFunction(FullMethodPath(method), MakeValue(arg1));
  }

  template<typename A1, typename A2>
  void CallJS(const std::string& method, const A1& arg1, const A2& arg2) {
    web_ui()->CallJavascriptFunction(FullMethodPath(method), MakeValue(arg1),
                                     MakeValue(arg2));
  }

  template<typename A1, typename A2, typename A3>
  void CallJS(const std::string& method,
              const A1& arg1,
              const A2& arg2,
              const A3& arg3) {
    web_ui()->CallJavascriptFunction(FullMethodPath(method),
                                     MakeValue(arg1),
                                     MakeValue(arg2),
                                     MakeValue(arg3));
  }

  template<typename A1, typename A2, typename A3, typename A4>
  void CallJS(const std::string& method,
              const A1& arg1,
              const A2& arg2,
              const A3& arg3,
              const A4& arg4) {
    web_ui()->CallJavascriptFunction(FullMethodPath(method),
                                     MakeValue(arg1),
                                     MakeValue(arg2),
                                     MakeValue(arg3),
                                     MakeValue(arg4));
  }

  // Shortcut methods for adding WebUI callbacks.
  template<typename T>
  void AddRawCallback(const std::string& name,
                      void (T::*method)(const base::ListValue* args)) {
    web_ui()->RegisterMessageCallback(
        name,
        base::Bind(method, base::Unretained(static_cast<T*>(this))));
  }

  template<typename T>
  void AddCallback(const std::string& name, void (T::*method)()) {
    base::Callback<void()> callback =
        base::Bind(method, base::Unretained(static_cast<T*>(this)));
    web_ui()->RegisterMessageCallback(
        name, base::Bind(&CallbackWrapper0, callback));
  }

  template<typename T, typename A1>
  void AddCallback(const std::string& name, void (T::*method)(A1 arg1)) {
    base::Callback<void(A1)> callback =
        base::Bind(method, base::Unretained(static_cast<T*>(this)));
    web_ui()->RegisterMessageCallback(
        name, base::Bind(&CallbackWrapper1<A1>, callback));
  }

  template<typename T, typename A1, typename A2>
  void AddCallback(const std::string& name,
                   void (T::*method)(A1 arg1, A2 arg2)) {
    base::Callback<void(A1, A2)> callback =
        base::Bind(method, base::Unretained(static_cast<T*>(this)));
    web_ui()->RegisterMessageCallback(
        name, base::Bind(&CallbackWrapper2<A1, A2>, callback));
  }

  template<typename T, typename A1, typename A2, typename A3>
  void AddCallback(const std::string& name,
                   void (T::*method)(A1 arg1, A2 arg2, A3 arg3)) {
    base::Callback<void(A1, A2, A3)> callback =
        base::Bind(method, base::Unretained(static_cast<T*>(this)));
    web_ui()->RegisterMessageCallback(
        name, base::Bind(&CallbackWrapper3<A1, A2, A3>, callback));
  }

  template<typename T, typename A1, typename A2, typename A3, typename A4>
  void AddCallback(const std::string& name,
                   void (T::*method)(A1 arg1, A2 arg2, A3 arg3, A4 arg4)) {
    base::Callback<void(A1, A2, A3, A4)> callback =
        base::Bind(method, base::Unretained(static_cast<T*>(this)));
    web_ui()->RegisterMessageCallback(
        name, base::Bind(&CallbackWrapper4<A1, A2, A3, A4>, callback));
  }

  template <typename Method>
  void AddPrefixedCallback(const std::string& unprefixed_name,
                           const Method& method) {
    AddCallback(FullMethodPath(unprefixed_name), method);
  }

  // Called when the page is ready and handler can do initialization.
  virtual void Initialize() = 0;

  // Show selected WebUI |screen|. Optionally it can pass screen initialization
  // data via |data| parameter.
  void ShowScreen(const char* screen, const base::DictionaryValue* data);

  // Whether page is ready.
  bool page_is_ready() const { return page_is_ready_; }

  // Returns the window which shows us.
  virtual gfx::NativeWindow GetNativeWindow();

 private:
  // Returns full name of JS method based on screen and method
  // names.
  std::string FullMethodPath(const std::string& method) const;

  // Keeps whether page is ready.
  bool page_is_ready_;

  base::DictionaryValue* localized_values_;

  // Full name of the corresponding JS screen object. Can be empty, if
  // there are no corresponding screen object or several different
  // objects.
  std::string js_screen_path_prefix_;

  // The string id used in the async asset load in JS. If it is set to a
  // non empty value, the Initialize will be deferred until the underlying load
  // is finished.
  std::string async_assets_load_id_;

  DISALLOW_COPY_AND_ASSIGN(BaseScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_
