// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for spellcheck.
// Multiply-included message file, hence no include guard.

#include "chrome/common/spellcheck_result.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"


#define IPC_MESSAGE_START SpellCheckMsgStart

IPC_ENUM_TRAITS(SpellCheckResult::Type)

IPC_STRUCT_TRAITS_BEGIN(SpellCheckResult)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(location)
  IPC_STRUCT_TRAITS_MEMBER(length)
  IPC_STRUCT_TRAITS_MEMBER(replacement)
IPC_STRUCT_TRAITS_END()

// Messages sent from the browser to the renderer.

IPC_MESSAGE_CONTROL1(SpellCheckMsg_EnableSpellCheck,
                    bool)

// Passes some initialization params to the renderer's spellchecker. This can
// be called directly after startup or in (async) response to a
// RequestDictionary ViewHost message.
IPC_MESSAGE_CONTROL4(SpellCheckMsg_Init,
                     IPC::PlatformFileForTransit /* bdict_file */,
                     std::vector<std::string> /* custom_dict_words */,
                     std::string /* language */,
                     bool /* auto spell correct */)

// Words have been added and removed in the custom dictionary; update the local
// custom word list.
IPC_MESSAGE_CONTROL2(SpellCheckMsg_CustomDictionaryChanged,
                     std::vector<std::string> /* words_added */,
                     std::vector<std::string> /* words_removed */)

// Toggle the auto spell correct functionality.
IPC_MESSAGE_CONTROL1(SpellCheckMsg_EnableAutoSpellCorrect,
                     bool /* enable */)

#if !defined(OS_MACOSX)
// Sends text-check results from the Spelling service when the service finishes
// checking text received by a SpellCheckHostMsg_CallSpellingService message.
// If the service is not available, the 4th parameter should be false and the
// 5th parameter should contain the requested sentence.
IPC_MESSAGE_ROUTED5(SpellCheckMsg_RespondSpellingService,
                    int         /* request identifier given by WebKit */,
                    int         /* offset */,
                    bool        /* succeeded calling service */,
                    string16    /* sentence */,
                    std::vector<SpellCheckResult>)
#endif

#if defined(OS_MACOSX)
// This message tells the renderer to advance to the next misspelling. It is
// sent when the user clicks the "Find Next" button on the spelling panel.
IPC_MESSAGE_ROUTED0(SpellCheckMsg_AdvanceToNextMisspelling)

// Sends when NSSpellChecker finishes checking text received by a preceding
// SpellCheckHostMsg_RequestTextCheck message.
IPC_MESSAGE_ROUTED2(SpellCheckMsg_RespondTextCheck,
                    int        /* request identifier given by WebKit */,
                    std::vector<SpellCheckResult>)

IPC_MESSAGE_ROUTED1(SpellCheckMsg_ToggleSpellPanel,
                    bool)
#endif

// Messages sent from the renderer to the browser.

// The renderer has tried to spell check a word, but couldn't because no
// dictionary was available to load. Request that the browser find an
// appropriate dictionary and return it.
IPC_MESSAGE_CONTROL0(SpellCheckHostMsg_RequestDictionary)

// Tracks spell checking occurrence to collect histogram.
IPC_MESSAGE_ROUTED2(SpellCheckHostMsg_NotifyChecked,
                    string16 /* word */,
                    bool /* true if checked word is misspelled */)

#if !defined(OS_MACOSX)
// Asks the Spelling service to check text. When the service finishes checking
// the input text, it sends a SpellingCheckMsg_RespondSpellingService with
// text-check results.
IPC_MESSAGE_CONTROL4(SpellCheckHostMsg_CallSpellingService,
                     int /* route_id for response */,
                     int /* request identifier given by WebKit */,
                     int /* offset */,
                     string16 /* sentence */)
#endif

#if defined(OS_MACOSX)
// This message tells the spellchecker that a document has been closed and all
// of the ignored words for that document can be forgotten.
IPC_MESSAGE_ROUTED1(SpellCheckHostMsg_DocumentClosed,
                    int /* route_id to identify document */)

// Tells the browser to display or not display the SpellingPanel
IPC_MESSAGE_ROUTED1(SpellCheckHostMsg_ShowSpellingPanel,
                    bool /* if true, then show it, otherwise hide it*/)

// Tells the browser to update the spelling panel with the given word.
IPC_MESSAGE_ROUTED1(SpellCheckHostMsg_UpdateSpellingPanelWithMisspelledWord,
                    string16 /* the word to update the panel with */)

// TODO(groby): This needs to originate from SpellcheckProvider.
IPC_SYNC_MESSAGE_CONTROL2_1(SpellCheckHostMsg_CheckSpelling,
                            string16 /* word */,
                            int /* route_id */,
                            bool /* correct */)

IPC_SYNC_MESSAGE_CONTROL1_1(SpellCheckHostMsg_FillSuggestionList,
                            string16 /* word */,
                            std::vector<string16> /* suggestions */)

IPC_MESSAGE_CONTROL3(SpellCheckHostMsg_RequestTextCheck,
                     int /* route_id for response */,
                     int /* request identifier given by WebKit */,
                     string16 /* sentence */)

IPC_MESSAGE_ROUTED2(SpellCheckHostMsg_ToggleSpellCheck,
                    bool /* enabled */,
                    bool /* checked */)
#endif  // OS_MACOSX
