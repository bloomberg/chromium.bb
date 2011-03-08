// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_modal_dialogs/message_box_handler.h"

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/app_modal_dialogs/js_modal_dialog.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/message_box_flags.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/font.h"

static std::wstring GetTitle(Profile* profile,
                             bool is_alert,
                             const GURL& frame_url) {
  ExtensionService* extensions_service = profile->GetExtensionService();
  if (extensions_service) {
    const Extension* extension =
        extensions_service->GetExtensionByURL(frame_url);
    if (!extension)
      extension = extensions_service->GetExtensionByWebExtent(frame_url);

    if (extension && (extension->location() == Extension::COMPONENT)) {
      return UTF16ToWideHack(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
    } else if (extension && !extension->name().empty()) {
      return UTF8ToWide(extension->name());
    }
  }
  if (!frame_url.has_host()) {
    return UTF16ToWideHack(l10n_util::GetStringUTF16(
        is_alert ? IDS_JAVASCRIPT_ALERT_DEFAULT_TITLE
                 : IDS_JAVASCRIPT_MESSAGEBOX_DEFAULT_TITLE));
  }

  // TODO(brettw) it should be easier than this to do the correct language
  // handling without getting the accept language from the profile.
  string16 base_address = ui::ElideUrl(frame_url.GetOrigin(),
      gfx::Font(), 0, profile->GetPrefs()->GetString(prefs::kAcceptLanguages));

  // Force URL to have LTR directionality.
  base_address = base::i18n::GetDisplayStringInLTRDirectionality(
      base_address);

  return UTF16ToWide(l10n_util::GetStringFUTF16(
      is_alert ? IDS_JAVASCRIPT_ALERT_TITLE :
                 IDS_JAVASCRIPT_MESSAGEBOX_TITLE,
      base_address));
}

void RunJavascriptMessageBox(Profile* profile,
                             JavaScriptAppModalDialogDelegate* delegate,
                             const GURL& frame_url,
                             int dialog_flags,
                             const std::wstring& message_text,
                             const std::wstring& default_prompt_text,
                             bool display_suppress_checkbox,
                             IPC::Message* reply_msg) {
  bool is_alert = dialog_flags == ui::MessageBoxFlags::kIsJavascriptAlert;
  std::wstring title = GetTitle(profile, is_alert, frame_url);
  AppModalDialogQueue::GetInstance()->AddDialog(new JavaScriptAppModalDialog(
      delegate, title, dialog_flags, message_text, default_prompt_text,
      display_suppress_checkbox, false, reply_msg));
}

void RunBeforeUnloadDialog(TabContents* tab_contents,
                           const std::wstring& message_text,
                           IPC::Message* reply_msg) {
  std::wstring full_message = message_text + L"\n\n" + UTF16ToWideHack(
      l10n_util::GetStringUTF16(IDS_BEFOREUNLOAD_MESSAGEBOX_FOOTER));
  AppModalDialogQueue::GetInstance()->AddDialog(new JavaScriptAppModalDialog(
      tab_contents,
      UTF16ToWideHack(
          l10n_util::GetStringUTF16(IDS_BEFOREUNLOAD_MESSAGEBOX_TITLE)),
      ui::MessageBoxFlags::kIsJavascriptConfirm,
      message_text,
      std::wstring(),
      false,
      true,
      reply_msg));
}
