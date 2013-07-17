// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/context_menu_params_builder.h"

#include "base/logging.h"
#include "content/common/ssl_status_serialization.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/renderer/history_item_serialization.h"
#include "content/renderer/menu_item_builder.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebNode.h"

namespace content {

// static
ContextMenuParams ContextMenuParamsBuilder::Build(
    const WebKit::WebContextMenuData& data) {
  ContextMenuParams params;
  params.media_type = data.mediaType;
  params.x = data.mousePosition.x;
  params.y = data.mousePosition.y;
  params.link_url = data.linkURL;
  params.unfiltered_link_url = data.linkURL;
  params.src_url = data.srcURL;
  params.is_image_blocked = data.isImageBlocked;
  params.page_url = data.pageURL;
  params.keyword_url = data.keywordURL;
  params.frame_url = data.frameURL;
  params.media_flags = data.mediaFlags;
  params.selection_text = data.selectedText;
  params.misspelled_word = data.misspelledWord;
  params.misspelling_hash = data.misspellingHash;
  params.speech_input_enabled = data.isSpeechInputEnabled;
  params.spellcheck_enabled = data.isSpellCheckingEnabled;
  params.is_editable = data.isEditable;
#if defined(OS_MACOSX)
  params.writing_direction_default = data.writingDirectionDefault;
  params.writing_direction_left_to_right = data.writingDirectionLeftToRight;
  params.writing_direction_right_to_left = data.writingDirectionRightToLeft;
#endif  // OS_MACOSX
  params.edit_flags = data.editFlags;
  params.frame_charset = data.frameEncoding.utf8();
  params.referrer_policy = data.referrerPolicy;

  for (size_t i = 0; i < data.dictionarySuggestions.size(); ++i)
    params.dictionary_suggestions.push_back(data.dictionarySuggestions[i]);

  params.custom_context.is_pepper_menu = false;
  for (size_t i = 0; i < data.customItems.size(); ++i)
    params.custom_items.push_back(MenuItemBuilder::Build(data.customItems[i]));

  if (!data.frameHistoryItem.isNull())
    params.frame_page_state = HistoryItemToPageState(data.frameHistoryItem);

  if (!params.link_url.is_empty()) {
    WebKit::WebNode selectedNode = data.node;

    // If there are other embedded tags (like <a ..>Some <b>text</b></a>)
    // we need to extract the parent <a/> node.
    while (!selectedNode.isLink() && !selectedNode.parentNode().isNull()) {
      selectedNode = selectedNode.parentNode();
    }

    WebKit::WebElement selectedElement = selectedNode.to<WebKit::WebElement>();
    if (selectedNode.isLink() && !selectedElement.isNull()) {
      params.link_text = selectedElement.innerText();
    } else {
      LOG(ERROR) << "Creating a ContextMenuParams for a node that has a link"
                 << "url but is not an ElementNode or does not have an"
                 << "ancestor that is a link.";
    }
  }

  // Deserialize the SSL info.
  if (!data.securityInfo.isEmpty()) {
    DeserializeSecurityInfo(data.securityInfo,
                            &params.security_info.cert_id,
                            &params.security_info.cert_status,
                            &params.security_info.security_bits,
                            &params.security_info.connection_status);
  }

  return params;
}

}  // namespace content
