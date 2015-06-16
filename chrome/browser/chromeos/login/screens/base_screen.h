// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
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
// method called just once. However if initialization is too expensive, screen
// can override result of IsPermanent() method, and do clean-up upon subsequent
// Initialize() method calls.
class BaseScreen {
 public:
  explicit BaseScreen(BaseScreenDelegate* base_screen_delegate);
  virtual ~BaseScreen();

  // ---- Old implementation ----

  virtual void PrepareToShow() = 0;

  // Makes wizard screen visible.
  virtual void Show() = 0;

  // Makes wizard screen invisible.
  virtual void Hide() = 0;

  // Returns the screen name.
  virtual std::string GetName() const = 0;

  // ---- New Implementation ----

  // Called to perform initialization of the screen. UI is guaranteed to exist
  // at this point. Screen can alter context, resulting context will be passed
  // to JS. This method will be called once per instance of the Screen object,
  // unless |IsPermanent()| returns |true|.
  virtual void Initialize(::login::ScreenContext* context);

  // Called when screen appears.
  virtual void OnShow();
  // Called when screen disappears, either because it finished it's work, or
  // because some other screen pops up.
  virtual void OnHide();

  // Called when we navigate from screen so that we will never return to it.
  // This is a last chance to call JS counterpart, this object will be deleted
  // soon.
  virtual void OnClose();

  // Indicates whether status area should be displayed while this screen is
  // displayed.
  virtual bool IsStatusAreaDisplayed();

  // If this method returns |true|, screen will not be deleted once we leave it.
  // However, Initialize() might be called several times in this case.
  virtual bool IsPermanent();

  // Returns the identifier of the screen.
  virtual std::string GetID() const;

  // Called when user action event with |event_id|
  // happened. Notification about this event comes from the JS
  // counterpart.
  virtual void OnUserAction(const std::string& action_id);

  void set_model_view_channel(ModelViewChannel* channel) { channel_ = channel; }

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

  // Screen can call this method to notify framework that it have finished
  // it's work with |outcome|.
  void Finish(BaseScreenDelegate::ExitCodes exit_code);

  // The method is called each time some key in screen context is
  // updated by JS side. Default implementation does nothing, so
  // subclasses should override it in order to observe updates in
  // screen context.
  virtual void OnContextKeyUpdated(const ::login::ScreenContext::KeyType& key);

  // Returns scoped context editor. The editor or it's copies should not outlive
  // current BaseScreen instance.
  ContextEditor GetContextEditor();

  BaseScreenDelegate* get_base_screen_delegate() const {
    return base_screen_delegate_;
  }

  ::login::ScreenContext context_;

 private:
  FRIEND_TEST_ALL_PREFIXES(EnrollmentScreenTest, TestCancel);
  FRIEND_TEST_ALL_PREFIXES(EnrollmentScreenTest, TestSuccess);
  FRIEND_TEST_ALL_PREFIXES(ProvisionedEnrollmentScreenTest, TestBackButton);

  friend class BaseScreenHandler;
  friend class NetworkScreenTest;
  friend class ScreenEditor;
  friend class ScreenManager;
  friend class UpdateScreenTest;

  void SetContext(::login::ScreenContext* context);

  // Called when context for the current screen was
  // changed. Notification about this event comes from the JS
  // counterpart.
  void OnContextChanged(const base::DictionaryValue& diff);

  ModelViewChannel* channel_;

  BaseScreenDelegate* base_screen_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BaseScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_BASE_SCREEN_H_
