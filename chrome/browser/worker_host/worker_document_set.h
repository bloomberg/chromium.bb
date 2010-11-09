// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WORKER_HOST_WORKER_DOCUMENT_SET_H_
#define CHROME_BROWSER_WORKER_HOST_WORKER_DOCUMENT_SET_H_
#pragma once

#include <set>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "ipc/ipc_message.h"

// The WorkerDocumentSet tracks all of the DOM documents associated with a
// set of workers. With nested workers, multiple workers can share the same
// WorkerDocumentSet (meaning that they all share the same lifetime, and will
// all exit when the last document in that set exits, per the WebWorkers spec).
class WorkerDocumentSet : public base::RefCounted<WorkerDocumentSet> {
 public:
  WorkerDocumentSet();

  // The information we track for each document
  class DocumentInfo {
   public:
    DocumentInfo(IPC::Message::Sender* sender, unsigned long long document_id,
                 int renderer_id, int render_view_route_id);
    IPC::Message::Sender* sender() const { return sender_; }
    unsigned long long document_id() const { return document_id_; }
    int renderer_id() const { return renderer_id_; }
    int render_view_route_id() const { return render_view_route_id_; }

    // Define operator "<", which is used to determine uniqueness within
    // the set.
    bool operator <(const DocumentInfo& other) const {
      // Items are identical if the sender and document_id are identical,
      // otherwise create an arbitrary stable ordering based on the document
      // id/sender.
      if (sender() == other.sender()) {
        return document_id() < other.document_id();
      } else {
        return reinterpret_cast<unsigned long long>(sender()) <
            reinterpret_cast<unsigned long long>(other.sender());
      }
    }

   private:
    IPC::Message::Sender* sender_;
    unsigned long long document_id_;
    int renderer_id_;
    int render_view_route_id_;
  };

  // Adds a document to a shared worker's document set. Also includes the
  // associated renderer_id the document is associated with, to enable
  // communication with the parent tab for things like http auth dialogs.
  void Add(IPC::Message::Sender* parent,
           unsigned long long document_id,
           int renderer_id,
           int render_view_route_id);

  // Checks to see if a document is in a shared worker's document set.
  bool Contains(IPC::Message::Sender* parent,
                unsigned long long document_id) const;

  // Removes a specific document from a worker's document set when that document
  // is detached.
  void Remove(IPC::Message::Sender* parent, unsigned long long document_id);

  // Invoked when a render process exits, to remove all associated documents
  // from a worker's document set.
  void RemoveAll(IPC::Message::Sender* parent);

  bool IsEmpty() const { return document_set_.empty(); }

  // Define a typedef for convenience here when declaring iterators, etc.
  typedef std::set<DocumentInfo> DocumentInfoSet;

  // Returns the set of documents associated with this worker.
  const DocumentInfoSet& documents() { return document_set_; }

 private:
  friend class base::RefCounted<WorkerDocumentSet>;
  virtual ~WorkerDocumentSet();

  DocumentInfoSet document_set_;
};

#endif  // CHROME_BROWSER_WORKER_HOST_WORKER_DOCUMENT_SET_H_
