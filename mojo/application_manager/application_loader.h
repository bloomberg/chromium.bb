// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPLICATION_MANAGER_APPLICATION_LOADER_H_
#define MOJO_APPLICATION_MANAGER_APPLICATION_LOADER_H_

#include "base/memory/ref_counted.h"
#include "mojo/application_manager/application_manager_export.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/services/public/interfaces/network/url_loader.mojom.h"
#include "url/gurl.h"

namespace mojo {

class ApplicationManager;

// Interface to allowing loading behavior to be established for schemes,
// specific urls or as the default.
// A ApplicationLoader is responsible to using whatever mechanism is appropriate
// to load the application at url.
// The handle to the shell is passed to that application so it can bind it to
// a Shell instance. This will give the Application a way to connect to other
// apps and services.
class MOJO_APPLICATION_MANAGER_EXPORT ApplicationLoader {
 public:
  class MOJO_APPLICATION_MANAGER_EXPORT LoadCallbacks
      : public base::RefCounted<LoadCallbacks> {
   public:
    // Register the requested application with ApplicationManager. If the
    // returned handle is valid, it should be used to implement the
    // mojo::Application interface.
    virtual ScopedMessagePipeHandle RegisterApplication() = 0;

    // Load the requested application with a content handler.
    virtual void LoadWithContentHandler(const GURL& content_handler_url,
                                        URLResponsePtr response) = 0;

   protected:
    friend base::RefCounted<LoadCallbacks>;
    virtual ~LoadCallbacks() {}
  };

  // Implements RegisterApplication() by returning a handle that was specified
  // at construction time. LoadWithContentHandler() is not supported.
  class MOJO_APPLICATION_MANAGER_EXPORT SimpleLoadCallbacks
      : public LoadCallbacks {
   public:
    SimpleLoadCallbacks(ScopedMessagePipeHandle shell_handle);
    virtual ScopedMessagePipeHandle RegisterApplication() OVERRIDE;
    virtual void LoadWithContentHandler(const GURL& content_handler_url,
                                        URLResponsePtr response) OVERRIDE;

   private:
    ScopedMessagePipeHandle shell_handle_;
    virtual ~SimpleLoadCallbacks();
  };

  virtual ~ApplicationLoader() {}

  // Load the application named |url|. Applications can be loaded two ways:
  //
  // 1. |url| can refer directly to a Mojo application. In this case, call
  //    callbacks->RegisterApplication(). The returned handle should be used to
  //    implement the mojo.Application interface. Note that the returned handle
  //    can be invalid in the case where the application has already been
  //    loaded.
  //
  // 2. |url| can refer to some content that can be handled by some other Mojo
  //    application. In this case, call callbacks->LoadWithContentHandler() and
  //    specify the URL of the application that should handle the content.
  //    The specified application must implement the mojo.ContentHandler
  //    interface.
  virtual void Load(ApplicationManager* application_manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) = 0;

  virtual void OnServiceError(ApplicationManager* manager, const GURL& url) = 0;

 protected:
  ApplicationLoader() {}
};

}  // namespace mojo

#endif  // MOJO_APPLICATION_MANAGER_APPLICATION_LOADER_H_
