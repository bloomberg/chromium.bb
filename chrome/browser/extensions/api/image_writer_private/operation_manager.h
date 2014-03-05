// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_OPERATION_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_OPERATION_MANAGER_H_

#include <map>
#include <string>
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/api/image_writer_private/image_writer_private_api.h"
#include "chrome/browser/extensions/api/image_writer_private/operation.h"
#include "chrome/common/extensions/api/image_writer_private.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "url/gurl.h"

namespace image_writer_api = extensions::api::image_writer_private;

class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {
namespace image_writer {

class Operation;

// Manages image writer operations for the current profile.  Including clean-up
// and message routing.
class OperationManager : public BrowserContextKeyedAPI,
                         public content::NotificationObserver,
                         public base::SupportsWeakPtr<OperationManager> {
 public:
  typedef std::string ExtensionId;

  explicit OperationManager(content::BrowserContext* context);
  virtual ~OperationManager();

  virtual void Shutdown() OVERRIDE;

  // Starts a WriteFromUrl operation.
  void StartWriteFromUrl(const ExtensionId& extension_id,
                         GURL url,
                         const std::string& hash,
                         const std::string& device_path,
                         const Operation::StartWriteCallback& callback);

  // Starts a WriteFromFile operation.
  void StartWriteFromFile(const ExtensionId& extension_id,
                          const base::FilePath& path,
                          const std::string& device_path,
                          const Operation::StartWriteCallback& callback);

  // Cancels the extensions current operation if any.
  void CancelWrite(const ExtensionId& extension_id,
                   const Operation::CancelWriteCallback& callback);

  // Starts a write that removes the partition table.
  void DestroyPartitions(const ExtensionId& extension_id,
                         const std::string& device_path,
                         const Operation::StartWriteCallback& callback);

  // Callback for progress events.
  virtual void OnProgress(const ExtensionId& extension_id,
                          image_writer_api::Stage stage,
                          int progress);
  // Callback for completion events.
  virtual void OnComplete(const ExtensionId& extension_id);

  // Callback for error events.
  virtual void OnError(const ExtensionId& extension_id,
                       image_writer_api::Stage stage,
                       int progress,
                       const std::string& error_message);

  // BrowserContextKeyedAPI
  static BrowserContextKeyedAPIFactory<OperationManager>* GetFactoryInstance();
  static OperationManager* Get(content::BrowserContext* context);

  Profile* profile() { return profile_; }

 private:

  static const char* service_name() {
    return "OperationManager";
  }

  // NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Operation* GetOperation(const ExtensionId& extension_id);
  void DeleteOperation(const ExtensionId& extension_id);

  friend class BrowserContextKeyedAPIFactory<OperationManager>;
  typedef std::map<ExtensionId, scoped_refptr<Operation> > OperationMap;

  Profile* profile_;
  OperationMap operations_;
  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<OperationManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(OperationManager);
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_OPERATION_MANAGER_H_
