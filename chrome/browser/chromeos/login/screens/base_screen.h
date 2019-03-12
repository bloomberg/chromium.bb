// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "components/login/base_screen_handler_utils.h"
#include "components/login/screens/screen_context.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class ModelViewChannel;

// Base class for the all OOBE/login/before-session screens.
// Screens are identified by ID, screen and it's JS counterpart must have same
// id.
// Most of the screens will be re-created for each appearance with Initialize()
// method called just once.
class BaseScreen {
 public:
  explicit BaseScreen(OobeScreen screen_id);
  virtual ~BaseScreen();

  // Makes wizard screen visible.
  virtual void Show() = 0;

  // Makes wizard screen invisible.
  virtual void Hide() = 0;

  // Returns the identifier of the screen.
  OobeScreen screen_id() const { return screen_id_; }

  // Called when user action event with |event_id|
  // happened. Notification about this event comes from the JS
  // counterpart.
  virtual void OnUserAction(const std::string& action_id);

  void set_model_view_channel(ModelViewChannel* channel) { channel_ = channel; }

  virtual void SetConfiguration(base::Value* configuration, bool notify);

 protected:
  // Scoped context editor, which automatically commits all pending
  // context changes on destruction.
  class ContextEditor {
   public:
    using KeyType = ::login::ScreenContext::KeyType;
    using String16List = ::login::String16List;
    using StringList = ::login::StringList;

    explicit ContextEditor(BaseScreen& screen);
    ~ContextEditor();

    const ContextEditor& SetBoolean(const KeyType& key, bool value) const;
    const ContextEditor& SetInteger(const KeyType& key, int value) const;
    const ContextEditor& SetDouble(const KeyType& key, double value) const;
    const ContextEditor& SetString(const KeyType& key,
                                   const std::string& value) const;
    const ContextEditor& SetString(const KeyType& key,
                                   const base::string16& value) const;
    const ContextEditor& SetStringList(const KeyType& key,
                                       const StringList& value) const;
    const ContextEditor& SetString16List(const KeyType& key,
                                         const String16List& value) const;

   private:
    BaseScreen& screen_;
    ::login::ScreenContext& context_;
  };

  // Sends all pending context changes to the JS side.
  void CommitContextChanges();

  // The method is called each time some key in screen context is
  // updated by JS side. Default implementation does nothing, so
  // subclasses should override it in order to observe updates in
  // screen context.
  virtual void OnContextKeyUpdated(const ::login::ScreenContext::KeyType& key);

  // Returns scoped context editor. The editor or it's copies should not outlive
  // current BaseScreen instance.
  ContextEditor GetContextEditor();

  // Global configuration for OOBE screens, that can be used to automate some
  // screens.
  // Screens can use values in Configuration to fill in UI values or
  // automatically finish.
  // Configuration is guaranteed to exist between pair of OnShow/OnHide calls,
  // no external changes will be made to configuration during that time.
  // Outside that time the configuration is set to nullptr to prevent any logic
  // triggering while the screen is not displayed.
  // Do not confuse it with Context, which is a way to communicate with
  // JS-based UI part of the screen.
  base::Value* GetConfiguration() { return configuration_; }

  // This is called when configuration is changed while screen is displayed.
  virtual void OnConfigurationChanged();

  ::login::ScreenContext context_;

 private:
  friend class BaseWebUIHandler;
  friend class EnrollmentScreenTest;
  friend class NetworkScreenTest;
  friend class ScreenManager;
  friend class UpdateScreenTest;

  // Called when context for the current screen was
  // changed. Notification about this event comes from the JS
  // counterpart.
  void OnContextChanged(const base::DictionaryValue& diff);

  // Screen configuration, unowned.
  // Configuration itself is owned by WizardController and is accessible
  // to screen only between OnShow / OnHide calls.
  base::Value* configuration_ = nullptr;

  ModelViewChannel* channel_ = nullptr;

  const OobeScreen screen_id_;

  DISALLOW_COPY_AND_ASSIGN(BaseScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_H_
