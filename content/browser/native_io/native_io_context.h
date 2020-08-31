// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_CONTEXT_H_
#define CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_CONTEXT_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-forward.h"
#include "url/origin.h"

namespace content {

class NativeIOHost;

// Implements the NativeIO Web Platform feature for a StoragePartition.
//
// Each StoragePartition owns exactly one instance of this class. This class
// creates and destroys NativeIOHost instances to meet the demands for NativeIO
// from different origins.
//
// This class is not thread-safe, and all access to an instance must happen on
// the same sequence.
class CONTENT_EXPORT NativeIOContext {
 public:
  // |profile_root| is empty for in-memory (Incognito) profiles. Otherwise,
  // |profile_root| must point to an existing directory. NativeIO will store its
  // data in a subdirectory of the profile root.
  explicit NativeIOContext(const base::FilePath& profile_root);

  ~NativeIOContext();

  NativeIOContext(const NativeIOContext&) = delete;
  NativeIOContext& operator=(const NativeIOContext&) = delete;

  // Binds |receiver| to the NativeIOHost serving |origin|.
  //
  // |receiver| must belong to a frame or worker serving |origin|.
  void BindReceiver(const url::Origin& origin,
                    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver);

  // Called when a receiver disconnected from a NativeIOHost.
  //
  // |host| must be owned by this context. This method should only be called by
  // NativeIOHost.
  void OnHostReceiverDisconnect(NativeIOHost* host);

 private:
  // Computes the path to the directory storing an origin's NativeIO files.
  //
  // Returns an empty path if the origin isn't supported for NativeIO.
  base::FilePath RootPathForOrigin(const url::Origin& origin);

  std::map<url::Origin, std::unique_ptr<NativeIOHost>> hosts_;

  // Points to the root directory for NativeIO files.
  //
  // This path is empty for in-memory (Incognito) profiles.
  const base::FilePath root_path_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_CONTEXT_H_
