// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/worker_host/worker_document_set.h"

WorkerDocumentSet::WorkerDocumentSet() {
}

void WorkerDocumentSet::Add(IPC::Message::Sender* parent,
                            unsigned long long document_id,
                            int renderer_id,
                            int render_view_route_id) {
  DocumentInfo info(parent, document_id, renderer_id, render_view_route_id);
  document_set_.insert(info);
}

bool WorkerDocumentSet::Contains(IPC::Message::Sender* parent,
                                 unsigned long long document_id) const {
  for (DocumentInfoSet::const_iterator i = document_set_.begin();
       i != document_set_.end(); ++i) {
    if (i->sender() == parent && i->document_id() == document_id)
      return true;
  }
  return false;
}

void WorkerDocumentSet::Remove(IPC::Message::Sender* parent,
                               unsigned long long document_id) {
  for (DocumentInfoSet::iterator i = document_set_.begin();
       i != document_set_.end(); i++) {
    if (i->sender() == parent && i->document_id() == document_id) {
      document_set_.erase(i);
      break;
    }
  }
  // Should not be duplicate copies in the document set.
  DCHECK(!Contains(parent, document_id));
}

void WorkerDocumentSet::RemoveAll(IPC::Message::Sender* parent) {
  for (DocumentInfoSet::iterator i = document_set_.begin();
       i != document_set_.end();) {

    // Note this idiom is somewhat tricky - calling document_set_.erase(iter)
    // invalidates any iterators that point to the element being removed, so
    // bump the iterator beyond the item being removed before calling erase.
    if (i->sender() == parent) {
      DocumentInfoSet::iterator item_to_delete = i++;
      document_set_.erase(item_to_delete);
    } else {
      ++i;
    }
  }
}

WorkerDocumentSet::DocumentInfo::DocumentInfo(
    IPC::Message::Sender* sender, unsigned long long document_id,
    int renderer_id, int render_view_route_id)
    : sender_(sender),
      document_id_(document_id),
      renderer_id_(renderer_id),
      render_view_route_id_(render_view_route_id) {
}
