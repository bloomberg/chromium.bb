// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_DOCUMENT_SET_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_DOCUMENT_SET_H_
#pragma once

#include <set>

#include "base/basictypes.h"
#include "base/ref_counted.h"

class WorkerMessageFilter;

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
    DocumentInfo(WorkerMessageFilter* filter, unsigned long long document_id,
                 int renderer_process_id, int render_view_id);
    WorkerMessageFilter* filter() const { return filter_; }
    unsigned long long document_id() const { return document_id_; }
    int render_process_id() const { return render_process_id_; }
    int render_view_id() const { return render_view_id_; }

    // Define operator "<", which is used to determine uniqueness within
    // the set.
    bool operator <(const DocumentInfo& other) const {
      // Items are identical if the sender and document_id are identical,
      // otherwise create an arbitrary stable ordering based on the document
      // id/filter.
      if (filter() == other.filter()) {
        return document_id() < other.document_id();
      } else {
        return reinterpret_cast<unsigned long long>(filter()) <
            reinterpret_cast<unsigned long long>(other.filter());
      }
    }

   private:
    WorkerMessageFilter* filter_;
    unsigned long long document_id_;
    int render_process_id_;
    int render_view_id_;
  };

  // Adds a document to a shared worker's document set. Also includes the
  // associated render_process_id the document is associated with, to enable
  // communication with the parent tab for things like http auth dialogs.
  void Add(WorkerMessageFilter* parent,
           unsigned long long document_id,
           int render_process_id,
           int render_view_id);

  // Checks to see if a document is in a shared worker's document set.
  bool Contains(WorkerMessageFilter* parent,
                unsigned long long document_id) const;

  // Removes a specific document from a worker's document set when that document
  // is detached.
  void Remove(WorkerMessageFilter* parent, unsigned long long document_id);

  // Invoked when a render process exits, to remove all associated documents
  // from a worker's document set.
  void RemoveAll(WorkerMessageFilter* parent);

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

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_DOCUMENT_SET_H_
