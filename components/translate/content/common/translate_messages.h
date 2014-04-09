// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

#if defined(CLD2_DYNAMIC_MODE)
#include "ipc/ipc_platform_file.h"
#endif

#define IPC_MESSAGE_START TranslateMsgStart

IPC_ENUM_TRAITS(TranslateErrors::Type)

IPC_STRUCT_TRAITS_BEGIN(LanguageDetectionDetails)
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
                    int /* page id */,
                    std::string, /* the script injected in the page */
                    std::string, /* BCP 47/RFC 5646 language code the page
                                    is in */
                    std::string /* BCP 47/RFC 5646 language code to translate
                                   to */)

#if defined(CLD2_DYNAMIC_MODE)
// Informs the renderer process that Compact Language Detector (CLD) data is
// available and provides an IPC::PlatformFileForTransit obtained from
// IPC::GetFileHandleForProcess(...)
// See also: ChromeViewHostMsg_NeedCLDData
IPC_MESSAGE_ROUTED3(ChromeViewMsg_CLDDataAvailable,
                    IPC::PlatformFileForTransit /* ipc_file_handle */,
                    uint64 /* data_offset */,
                    uint64 /* data_length */)
#endif

//-----------------------------------------------------------------------------
// Misc messages
// These are messages sent from the renderer to the browser process.

// Notification that the language for the tab has been determined.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_TranslateLanguageDetermined,
                    LanguageDetectionDetails /* details about lang detection */,
                    bool /* whether the page needs translation */)

// Notifies the browser that a page has been translated.
IPC_MESSAGE_ROUTED4(ChromeViewHostMsg_PageTranslated,
                    int,                  /* page id */
                    std::string           /* the original language */,
                    std::string           /* the translated language */,
                    TranslateErrors::Type /* the error type if available */)

// Tells the renderer to revert the text of translated page to its original
// contents.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_RevertTranslation,
                    int /* page id */)

#if defined(CLD2_DYNAMIC_MODE)
// Informs the browser process that Compact Language Detector (CLD) data is
// required by the originating renderer. The browser process should respond
// with a ChromeViewMsg_CLDDataAvailable if the data is available, else it
// should go unanswered (the renderer will ask again later).
// See also: ChromeViewMsg_CLDDataAvailable
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_NeedCLDData)
#endif
