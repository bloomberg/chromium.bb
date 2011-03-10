// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "model_test_utils.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "googleurl/src/gurl.h"

namespace model_test_utils {

std::string ModelStringFromNode(const BookmarkNode* node) {
  // Since the children of the node are not available as a vector,
  // we'll just have to do it the hard way.
  int child_count = node->child_count();
  std::string child_string;
  for (int i = 0; i < child_count; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    if (child->is_folder()) {
      child_string += UTF16ToUTF8(child->GetTitle()) + ":[ " +
          ModelStringFromNode(child) + "] ";
    } else {
      child_string += UTF16ToUTF8(child->GetTitle()) + " ";
    }
  }
  return child_string;
}

// Helper function which does the actual work of creating the nodes for
// a particular level in the hierarchy.
std::string::size_type AddNodesFromString(BookmarkModel& model,
                                          const BookmarkNode* node,
                                          const std::string& model_string,
                                          std::string::size_type start_pos) {
  DCHECK(node);
  int index = node->child_count();
  static const std::string folder_tell(":[");
  std::string::size_type end_pos = model_string.find(' ', start_pos);
  while (end_pos != std::string::npos) {
    std::string::size_type part_length = end_pos - start_pos;
    std::string node_name = model_string.substr(start_pos, part_length);
    // Are we at the end of a folder group?
    if (node_name != "]") {
      // No, is it a folder?
      std::string tell;
      if (part_length > 2)
        tell = node_name.substr(part_length - 2, 2);
      if (tell == folder_tell) {
        node_name = node_name.substr(0, part_length - 2);
        const BookmarkNode* new_node =
            model.AddGroup(node, index, UTF8ToUTF16(node_name));
        end_pos = AddNodesFromString(model, new_node, model_string,
                                     end_pos + 1);
      } else {
        std::string url_string("http://");
        url_string += std::string(node_name.begin(), node_name.end());
        url_string += ".com";
        model.AddURL(node, index, UTF8ToUTF16(node_name), GURL(url_string));
        ++end_pos;
      }
      ++index;
      start_pos = end_pos;
      end_pos = model_string.find(' ', start_pos);
    } else {
      ++end_pos;
      break;
    }
  }
  return end_pos;
}

void AddNodesFromModelString(BookmarkModel& model,
                             const BookmarkNode* node,
                             const std::string& model_string) {
  DCHECK(node);
  const std::string folder_tell(":[");
  std::string::size_type start_pos = 0;
  std::string::size_type end_pos =
      AddNodesFromString(model, node, model_string, start_pos);
  DCHECK(end_pos == std::string::npos);
}

}  // namespace model_test_utils
