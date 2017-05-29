// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for spellcheck.
// Multiply-included message file, hence no include guard.

#include <stdint.h>

#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "ipc/ipc_message_macros.h"

#if !BUILDFLAG(ENABLE_SPELLCHECK)
#error "Spellcheck should be enabled."
#endif

#define IPC_MESSAGE_START SpellCheckMsgStart

IPC_ENUM_TRAITS(SpellCheckResult::Decoration)

IPC_STRUCT_TRAITS_BEGIN(SpellCheckResult)
  IPC_STRUCT_TRAITS_MEMBER(decoration)
  IPC_STRUCT_TRAITS_MEMBER(location)
  IPC_STRUCT_TRAITS_MEMBER(length)
  IPC_STRUCT_TRAITS_MEMBER(replacement)
IPC_STRUCT_TRAITS_END()

// Messages sent from the browser to the renderer.

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
// Sends when NSSpellChecker finishes checking text received by a preceding
// SpellCheckHostMsg_RequestTextCheck message.
IPC_MESSAGE_ROUTED3(SpellCheckMsg_RespondTextCheck,
                    int /* request identifier given by WebKit */,
                    base::string16 /* sentence */,
                    std::vector<SpellCheckResult>)
#endif

#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
// This message tells the renderer to advance to the next misspelling. It is
// sent when the user clicks the "Find Next" button on the spelling panel.
IPC_MESSAGE_ROUTED0(SpellCheckMsg_AdvanceToNextMisspelling)

IPC_MESSAGE_ROUTED1(SpellCheckMsg_ToggleSpellPanel, bool)
#endif

// Messages sent from the renderer to the browser.

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
// TODO(groby): This needs to originate from SpellcheckProvider.
IPC_SYNC_MESSAGE_CONTROL2_1(SpellCheckHostMsg_CheckSpelling,
                            base::string16 /* word */,
                            int /* route_id */,
                            bool /* correct */)

IPC_SYNC_MESSAGE_CONTROL1_1(SpellCheckHostMsg_FillSuggestionList,
                            base::string16 /* word */,
                            std::vector<base::string16> /* suggestions */)

IPC_MESSAGE_CONTROL3(SpellCheckHostMsg_RequestTextCheck,
                     int /* route_id for response */,
                     int /* request identifier given by WebKit */,
                     base::string16 /* sentence */)

IPC_MESSAGE_ROUTED2(SpellCheckHostMsg_ToggleSpellCheck,
                    bool /* enabled */,
                    bool /* checked */)
#endif  // USE_BROWSER_SPELLCHECKER

#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
// Tells the browser to display or not display the SpellingPanel
IPC_MESSAGE_ROUTED1(SpellCheckHostMsg_ShowSpellingPanel,
                    bool /* if true, then show it, otherwise hide it*/)

// Tells the browser to update the spelling panel with the given word.
IPC_MESSAGE_ROUTED1(SpellCheckHostMsg_UpdateSpellingPanelWithMisspelledWord,
                    base::string16 /* the word to update the panel with */)
#endif
