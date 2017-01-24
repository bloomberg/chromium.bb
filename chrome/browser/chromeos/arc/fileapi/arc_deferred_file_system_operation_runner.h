// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DEFERRED_FILE_SYSTEM_OPERATION_RUNNER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DEFERRED_FILE_SYSTEM_OPERATION_RUNNER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/file_system.mojom.h"
#include "components/arc/file_system/arc_file_system_operation_runner.h"
#include "components/arc/instance_holder.h"

namespace arc {

// Implements deferred ARC file system operations.
//
// When ARC is disabled or ARC has already booted, file system operations are
// performed immediately. While ARC boot is under progress, file operations are
// deferred until ARC boot finishes or the user disables ARC.
//
// This file system operation runner provides better UX when the user attempts
// to perform file operations while ARC is booting. For example:
//
// - Media views are mounted in Files app soon after the user logs into
//   the system. If the user attempts to open media views before ARC boots,
//   a spinner is shown until file system gets ready because ReadDirectory
//   operations are deferred.
// - When an Android content URL is opened soon after the user logs into
//   the system (because the user opened the tab before they logged out for
//   instance), the tab keeps loading until ARC boot finishes, instead of
//   failing immediately.
//
// All member functions must be called on the UI thread.
class ArcDeferredFileSystemOperationRunner
    : public ArcFileSystemOperationRunner,
      public ArcSessionManager::Observer,
      public InstanceHolder<mojom::FileSystemInstance>::Observer {
 public:
  explicit ArcDeferredFileSystemOperationRunner(
      ArcBridgeService* bridge_service);
  ~ArcDeferredFileSystemOperationRunner() override;

  // ArcFileSystemOperationRunner overrides:
  void GetFileSize(const GURL& url,
                   const GetFileSizeCallback& callback) override;
  void OpenFileToRead(const GURL& url,
                      const OpenFileToReadCallback& callback) override;
  void GetDocument(const std::string& authority,
                   const std::string& document_id,
                   const GetDocumentCallback& callback) override;
  void GetChildDocuments(const std::string& authority,
                         const std::string& parent_document_id,
                         const GetChildDocumentsCallback& callback) override;

  // ArcSessionManager::Observer overrides:
  void OnArcOptInChanged(bool enabled) override;

  // InstanceHolder<mojom::FileSystemInstance>::Observer overrides:
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

 private:
  friend class ArcDeferredFileSystemOperationRunnerTest;

  // Constructor called from unit tests.
  ArcDeferredFileSystemOperationRunner(ArcBridgeService* bridge_service,
                                       bool observe_events);

  // Called whenever ARC states related to |should_defer_| are changed.
  void OnStateChanged();

  // Enables/disables deferring.
  void SetShouldDefer(bool should_defer);

  // Indicates if this instance should register observers to receive events.
  // Usually true, but set to false in unit tests.
  bool observe_events_;

  // Set to true if operations should be deferred at this moment.
  bool should_defer_ = true;

  // List of deferred operations.
  std::vector<base::Closure> deferred_operations_;

  base::WeakPtrFactory<ArcDeferredFileSystemOperationRunner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcDeferredFileSystemOperationRunner);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DEFERRED_FILE_SYSTEM_OPERATION_RUNNER_H_
