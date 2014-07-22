// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

#define IPC_MESSAGE_START TranslateMsgStart

IPC_ENUM_TRAITS(translate::TranslateErrors::Type)

IPC_STRUCT_TRAITS_BEGIN(translate::LanguageDetectionDetails)
  IPC_STRUCT_TRAITS_MEMBER(time)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(content_language)
  IPC_STRUCT_TRAITS_MEMBER(cld_language)
  IPC_STRUCT_TRAITS_MEMBER(is_cld_reliable)
  IPC_STRUCT_TRAITS_MEMBER(html_root_language)
  IPC_STRUCT_TRAITS_MEMBER(adopted_language)
  IPC_STRUCT_TRAITS_MEMBER(contents)
IPC_STRUCT_TRAITS_END()

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the browser to the renderer process.

// Tells the renderer to translate the page contents from one language to
// another.
IPC_MESSAGE_ROUTED4(ChromeViewMsg_TranslatePage,
                    int /* page_seq_no */,
                    std::string, /* the script injected in the page */
                    std::string, /* BCP 47/RFC 5646 language code the page
                                    is in */
                    std::string /* BCP 47/RFC 5646 language code to translate
                                   to */)

// Tells the renderer to revert the text of translated page to its original
// contents.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_RevertTranslation,
                    int /* page id */)

//-----------------------------------------------------------------------------
// Host messages
// These are messages sent from the renderer to the browser process.

// Notification that the current page was assigned a sequence number.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_TranslateAssignedSequenceNumber,
                    int /* page_seq_no */)

// Notification that the language for the tab has been determined.
IPC_MESSAGE_ROUTED2(
    ChromeViewHostMsg_TranslateLanguageDetermined,
    translate::LanguageDetectionDetails /* details about lang detection */,
    bool /* whether the page needs translation */)

// Notifies the browser that a page has been translated.
IPC_MESSAGE_ROUTED3(
    ChromeViewHostMsg_PageTranslated,
    std::string /* the original language */,
    std::string /* the translated language */,
    translate::TranslateErrors::Type /* the error type if available */)
