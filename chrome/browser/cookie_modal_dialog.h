// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COOKIE_MODAL_DIALOG_H_
#define CHROME_BROWSER_COOKIE_MODAL_DIALOG_H_

#include <string>

#include "base/ref_counted.h"
#include "chrome/browser/app_modal_dialog.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/cookie_prompt_modal_dialog_delegate.h"
#include "googleurl/src/gurl.h"

#if defined(OS_LINUX)
#include "app/gtk_signal.h"
#endif

#if defined(OS_MACOSX)
#if __OBJC__
@class NSWindow;
#else
class NSWindow;
#endif
#endif

class HostContentSettingsMap;
class PrefService;

#if defined(OS_LINUX)
typedef struct _GtkWidget GtkWidget;
typedef struct _GParamSpec GParamSpec;
#endif

// A controller+model class for cookie and local storage warning prompt.
// |NativeDialog| is a platform specific view.
class CookiePromptModalDialog : public AppModalDialog {
 public:
  enum DialogType {
    DIALOG_TYPE_COOKIE = 0,
    DIALOG_TYPE_LOCAL_STORAGE,
    DIALOG_TYPE_DATABASE,
    DIALOG_TYPE_APPCACHE
  };

  // A union of data necessary to determine the type of message box to
  // show.
  CookiePromptModalDialog(TabContents* tab_contents,
                          HostContentSettingsMap* host_content_settings_map,
                          const GURL& origin,
                          const std::string& cookie_line,
                          CookiePromptModalDialogDelegate* delegate);
  CookiePromptModalDialog(TabContents* tab_contents,
                          HostContentSettingsMap* host_content_settings_map,
                          const GURL& origin,
                          const string16& key,
                          const string16& value,
                          CookiePromptModalDialogDelegate* delegate);
  CookiePromptModalDialog(TabContents* tab_contents,
                          HostContentSettingsMap* host_content_settings_map,
                          const GURL& origin,
                          const string16& database_name,
                          const string16& display_name,
                          unsigned long estimated_size,
                          CookiePromptModalDialogDelegate* delegate);
  CookiePromptModalDialog(TabContents* tab_contents,
                          HostContentSettingsMap* host_content_settings_map,
                          const GURL& appcache_manifest_url,
                          CookiePromptModalDialogDelegate* delegate);
  virtual ~CookiePromptModalDialog();

  static void RegisterUserPrefs(PrefService* prefs);

  // AppModalDialog overrides.
#if defined(OS_LINUX) || defined(OS_MACOSX)
  virtual void CreateAndShowDialog();
#endif
  virtual int GetDialogButtons();
  virtual void AcceptWindow();
  virtual void CancelWindow();
  virtual bool IsValid();

#if defined(OS_MACOSX)
  virtual void CloseModalDialog();
#endif

  DialogType dialog_type() const { return dialog_type_; }
  const GURL& origin() const { return origin_; }
  const std::string& cookie_line() const { return cookie_line_; }
  const string16& local_storage_key() const { return local_storage_key_; }
  const string16& local_storage_value() const { return local_storage_value_; }
  const string16& database_name() const { return database_name_; }
  const string16& display_name() const { return display_name_; }
  unsigned long estimated_size() const { return estimated_size_; }
  const GURL& appcache_manifest_url() const { return appcache_manifest_url_; }
  TabContents* tab_contents() const { return tab_contents_; }

  // Implement CookiePromptModalDialogDelegate.
  virtual void AllowSiteData(bool remember, bool session_expire);
  virtual void BlockSiteData(bool remember);

 protected:
  // AppModalDialog overrides.
  virtual NativeDialog CreateNativeDialog();
#if defined(OS_LINUX)
  virtual void HandleDialogResponse(GtkDialog* dialog, gint response_id);
  CHROMEGTK_CALLBACK_1(CookiePromptModalDialog,
                       void,
                       OnExpanderActivate,
                       GParamSpec*);
#endif

 private:

#if defined(OS_MACOSX)
  NSWindow* dialog_;
#endif

  // Used to verify our request is still necessary and when the response should
  // persist.
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  const DialogType dialog_type_;

  // The origin connected to this request.
  const GURL origin_;

  // Which data members are relevant depends on the dialog_type.
  const std::string cookie_line_;
  const string16 local_storage_key_;
  const string16 local_storage_value_;
  const string16 database_name_;
  const string16 display_name_;
  unsigned long estimated_size_;
  const GURL appcache_manifest_url_;

  // The caller should provide a delegate in order to receive results
  // from this delegate.  Any time after calling one of these methods, the
  // delegate could be deleted
  CookiePromptModalDialogDelegate* delegate_;

#if defined(OS_LINUX)
  // The "remember this choice" radio button in the dialog.
  GtkWidget* remember_radio_;

  // The cookie view; we keep this to querry the result combobox.
  GtkWidget* cookie_view_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CookiePromptModalDialog);
};

#endif  // CHROME_BROWSER_COOKIE_MODAL_DIALOG_H_
