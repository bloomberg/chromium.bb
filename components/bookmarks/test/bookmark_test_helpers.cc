// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/test/bookmark_test_helpers.h"

#include <stddef.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "url/gurl.h"

namespace bookmarks {
namespace test {

namespace {

// BookmarkLoadObserver is used when blocking until the BookmarkModel finishes
// loading. As soon as the BookmarkModel finishes loading the message loop is
// quit.
class BookmarkLoadObserver : public BaseBookmarkModelObserver {
 public:
  explicit BookmarkLoadObserver(const base::Closure& quit_task);
  ~BookmarkLoadObserver() override;

 private:
  // BaseBookmarkModelObserver:
  void BookmarkModelChanged() override;
  void BookmarkModelLoaded(BookmarkModel* model, bool ids_reassigned) override;

  base::Closure quit_task_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkLoadObserver);
};

BookmarkLoadObserver::BookmarkLoadObserver(const base::Closure& quit_task)
    : quit_task_(quit_task) {}

BookmarkLoadObserver::~BookmarkLoadObserver() {}

void BookmarkLoadObserver::BookmarkModelChanged() {}

void BookmarkLoadObserver::BookmarkModelLoaded(BookmarkModel* model,
                                               bool ids_reassigned) {
  quit_task_.Run();
}

// Helper function which does the actual work of creating the nodes for
// a particular level in the hierarchy.
std::string::size_type AddNodesFromString(BookmarkModel* model,
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
            model->AddFolder(node, index, base::UTF8ToUTF16(node_name));
        end_pos = AddNodesFromString(model, new_node, model_string,
                                     end_pos + 1);
      } else {
        std::string url_string("http://");
        url_string += std::string(node_name.begin(), node_name.end());
        url_string += ".com";
        model->AddURL(
            node, index, base::UTF8ToUTF16(node_name), GURL(url_string));
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

}  // namespace

void WaitForBookmarkModelToLoad(BookmarkModel* model) {
  if (model->loaded())
    return;
  base::RunLoop run_loop;
  base::MessageLoop::ScopedNestableTaskAllower allow(
      base::MessageLoop::current());

  BookmarkLoadObserver observer(run_loop.QuitClosure());
  model->AddObserver(&observer);
  run_loop.Run();
  model->RemoveObserver(&observer);
  DCHECK(model->loaded());
}

std::string ModelStringFromNode(const BookmarkNode* node) {
  // Since the children of the node are not available as a vector,
  // we'll just have to do it the hard way.
  int child_count = node->child_count();
  std::string child_string;
  for (int i = 0; i < child_count; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    if (child->is_folder()) {
      child_string += base::UTF16ToUTF8(child->GetTitle()) + ":[ " +
          ModelStringFromNode(child) + "] ";
    } else {
      child_string += base::UTF16ToUTF8(child->GetTitle()) + " ";
    }
  }
  return child_string;
}

void AddNodesFromModelString(BookmarkModel* model,
                             const BookmarkNode* node,
                             const std::string& model_string) {
  DCHECK(node);
  std::string::size_type start_pos = 0;
  std::string::size_type end_pos =
      AddNodesFromString(model, node, model_string, start_pos);
  DCHECK(end_pos == std::string::npos);
}

}  // namespace test
}  // namespace bookmarks
