// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_SESSION_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_SESSION_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {

class DOMStorageContextImpl;
class DOMStorageContextWrapper;

// This class determines the lifetime of a session storage namespace and
// provides an interface to Clone() an existing session storage namespace. It
// may be used on any thread. See class comments for DOMStorageContextImpl for a
// larger overview.
class CONTENT_EXPORT DOMStorageSession {
 public:
  // Constructs a |DOMStorageSession| and allocates a new ID for it.
  static std::unique_ptr<DOMStorageSession> Create(
      scoped_refptr<DOMStorageContextWrapper> context);

  // Constructs a |DOMStorageSession| and assigns |namespace_id|
  // to it.
  static std::unique_ptr<DOMStorageSession> CreateWithNamespace(
      scoped_refptr<DOMStorageContextWrapper> context,
      const std::string& namespace_id);

  // Constructs a |DOMStorageSession| with id |namespace_id| by cloning
  // |namespace_id_to_clone|.
  static std::unique_ptr<DOMStorageSession> CloneFrom(
      scoped_refptr<DOMStorageContextWrapper> context,
      std::string namepace_id,
      const std::string& namepace_id_to_clone);

  ~DOMStorageSession();

  const std::string& namespace_id() const { return namespace_id_; }
  void SetShouldPersist(bool should_persist);
  bool should_persist() const;
  DOMStorageContextWrapper* context() const { return context_wrapper_.get(); }
  bool IsFromContext(DOMStorageContextWrapper* context);

  std::unique_ptr<DOMStorageSession> Clone();

 private:
  class SequenceHelper;

  // Creates the non-mojo version.
  DOMStorageSession(scoped_refptr<DOMStorageContextWrapper> context_wrapper,
                    scoped_refptr<DOMStorageContextImpl> context_impl,
                    std::string namespace_id);
  // Creates a mojo version.
  DOMStorageSession(scoped_refptr<DOMStorageContextWrapper> context,
                    std::string namespace_id);

  scoped_refptr<DOMStorageContextImpl> context_;
  scoped_refptr<DOMStorageContextWrapper> context_wrapper_;
  scoped_refptr<base::SequencedTaskRunner> mojo_task_runner_;
  std::string namespace_id_;
  bool should_persist_;

  // Contructed on constructing thread of DOMStorageSession, used and destroyed
  // on |mojo_task_runner_|.
  std::unique_ptr<SequenceHelper> sequence_helper_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageSession);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_SESSION_H_
