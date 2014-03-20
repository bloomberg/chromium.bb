// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTO_LOGIN_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTO_LOGIN_INFOBAR_DELEGATE_H_

#include <string>
#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/signin/signin_manager.h"
#include "components/auto_login_parser/auto_login_parser.h"

class PrefService;
class Profile;

namespace content {
class NavigationController;
}

// This is the actual infobar displayed to prompt the user to auto-login.
class AutoLoginInfoBarDelegate : public ConfirmInfoBarDelegate,
                                 public SigninManagerBase::Observer {
 public:
  struct Params {
    // Information from a parsed header.
    auto_login_parser::HeaderData header;

    // Username to display in the infobar indicating user to be logged in as.
    // This is initially fetched from sign-in on non-Android platforms. Note
    // that on Android this field is not used.
    std::string username;
  };

  // Creates an autologin infobar and delegate and adds the infobar to the
  // infobar service for |web_contents|.  Returns whether the infobar was
  // successfully added.
  static bool Create(content::WebContents* web_contents, const Params& params);

  // These methods are defined in downstream code.
  bool AttachAccount(JavaObjectWeakGlobalRef weak_java_translate_helper);
  void LoginSuccess(JNIEnv* env, jobject obj, jstring result);
  void LoginFailed(JNIEnv* env, jobject obj);
  void LoginDismiss(JNIEnv* env, jobject obj);

  // Register Android JNI bindings.
  static bool Register(JNIEnv* env);

 protected:
  AutoLoginInfoBarDelegate(const Params& params, Profile* profile);
  virtual ~AutoLoginInfoBarDelegate();

 private:
  // Enum values used for UMA histograms.
  enum Actions {
    SHOWN,       // The infobar was shown to the user.
    ACCEPTED,    // The user pressed the accept button.
    REJECTED,    // The user pressed the reject button.
    DISMISSED,   // The user pressed the close button.
    IGNORED,     // The user ignored the infobar.
    LEARN_MORE,  // The user clicked on the learn more link.
    HISTOGRAM_BOUNDING_VALUE
  };

  const std::string& realm() const { return params_.header.realm; }
  const std::string& account() const { return params_.header.account; }
  const std::string& args() const { return params_.header.args; }

  // ConfirmInfoBarDelegate:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual int GetIconID() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual AutoLoginInfoBarDelegate* AsAutoLoginInfoBarDelegate() OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // SigninManagerBase::Observer:
  virtual void GoogleSignedOut(const std::string& username) OVERRIDE;

  void RecordHistogramAction(Actions action);

  JavaObjectWeakGlobalRef weak_java_auto_login_delegate_;
  std::string user_;
  const Params params_;

  Profile* profile_;

  // Whether any UI controls in the infobar were pressed or not.
  bool button_pressed_;

  DISALLOW_COPY_AND_ASSIGN(AutoLoginInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTO_LOGIN_INFOBAR_DELEGATE_H_
