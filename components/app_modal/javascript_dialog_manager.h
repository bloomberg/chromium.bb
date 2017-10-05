// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_MODAL_JAVASCRIPT_DIALOG_MANAGER_H_
#define COMPONENTS_APP_MODAL_JAVASCRIPT_DIALOG_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "content/public/browser/javascript_dialog_manager.h"

namespace app_modal {

class JavaScriptDialogExtensionsClient;
class JavaScriptNativeDialogFactory;

class JavaScriptDialogManager : public content::JavaScriptDialogManager {
 public:
  static JavaScriptDialogManager* GetInstance();

  JavaScriptNativeDialogFactory* native_dialog_factory() {
    return native_dialog_factory_.get();
  }

  // Sets the JavaScriptNativeDialogFactory used to create platform specific
  // dialog window instances.
  void SetNativeDialogFactory(
      std::unique_ptr<JavaScriptNativeDialogFactory> factory);

  // JavaScript dialogs may be opened by an extensions/app, thus they need
  // access to extensions functionality. This sets a client interface to
  // access //extensions.
  void SetExtensionsClient(
      std::unique_ptr<JavaScriptDialogExtensionsClient> extensions_client);

  // Gets the title for a dialog.
  base::string16 GetTitle(content::WebContents* web_contents,
                          const GURL& alerting_frame_url);

  // JavaScriptDialogManager:
  void RunJavaScriptDialog(content::WebContents* web_contents,
                           const GURL& alerting_frame_url,
                           content::JavaScriptDialogType dialog_type,
                           const base::string16& message_text,
                           const base::string16& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override;
  void RunBeforeUnloadDialog(content::WebContents* web_contents,
                             bool is_reload,
                             DialogClosedCallback callback) override;
  bool HandleJavaScriptDialog(content::WebContents* web_contents,
                              bool accept,
                              const base::string16* prompt_override) override;
  void CancelDialogs(content::WebContents* web_contents,
                     bool reset_state) override;

 private:
  friend struct base::DefaultSingletonTraits<JavaScriptDialogManager>;

  JavaScriptDialogManager();
  ~JavaScriptDialogManager() override;

  // Wrapper around OnDialogClosed; logs UMA stats before continuing on.
  void OnBeforeUnloadDialogClosed(content::WebContents* web_contents,
                                  DialogClosedCallback callback,
                                  bool success,
                                  const base::string16& user_input);

  // Wrapper around a DialogClosedCallback so that we can intercept it before
  // passing it onto the original callback.
  void OnDialogClosed(content::WebContents* web_contents,
                      DialogClosedCallback callback,
                      bool success,
                      const base::string16& user_input);

  // Mapping between the WebContents and their extra data. The key
  // is a void* because the pointer is just a cookie and is never dereferenced.
  JavaScriptAppModalDialog::ExtraDataMap javascript_dialog_extra_data_;

  std::unique_ptr<JavaScriptNativeDialogFactory> native_dialog_factory_;
  std::unique_ptr<JavaScriptDialogExtensionsClient> extensions_client_;

  // Record a single create and close timestamp to track the time between
  // dialogs. (Since Javascript dialogs are modal, this is even accurate!)
  base::TimeTicks last_close_time_;
  base::TimeTicks last_creation_time_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptDialogManager);
};

}  // namespace app_modal

#endif  // COMPONENTS_APP_MODAL_JAVASCRIPT_DIALOG_MANAGER_H_
