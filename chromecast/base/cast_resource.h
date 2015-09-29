// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_CAST_RESOURCE_H_
#define CHROMECAST_BASE_CAST_RESOURCE_H_

#include "base/macros.h"

namespace chromecast {

// Interface for resources needed to run application.
class CastResource {
 public:
  // Resources necessary to run cast apps. CastResource may contain union of the
  // following types.
  // TODO(yucliu): Split video resources and graphic resources.
  enum Resource {
    kResourceNone = 0,
    // All resources necessary to render sounds, for example, audio pipeline,
    // speaker, etc.
    kResourceAudio = 1 << 0,
    // All resources necessary to render videos or images, for example, video
    // pipeline, primary graphics plane, display, etc.
    kResourceScreenPrimary = 1 << 1,
    // All resources necessary to render overlaid images, for example, secondary
    // graphics plane, LCD, etc.
    kResourceScreenSecondary = 1 << 2,
    // Collection of resources used for display only combined with bitwise or.
    kResourceDisplayOnly = (kResourceScreenPrimary | kResourceScreenSecondary),
    // Collection of all resources combined with bitwise or.
    kResourceAll =
        (kResourceAudio | kResourceScreenPrimary | kResourceScreenSecondary),
  };

  class Client {
   public:
    // Called when resource is created. CastResource should not be owned by
    // Client. It can be called from any thread.
    virtual void OnResourceAcquired(CastResource* cast_resource) = 0;
    // Called when part or all resources are released. It can be called from any
    // thread.
    //   |cast_resource| the CastResource that is released. The pointer may be
    //                   invalid. Client can't call functions with that pointer.
    //   |remain| the unreleased resource of CastResource. If kResourceNone is
    //            returned, Client will remove the resource from its watching
    //            list.
    virtual void OnResourceReleased(CastResource* cast_resource,
                                    Resource remain) = 0;

   protected:
    virtual ~Client() {}
  };

  void SetCastResourceClient(Client* client);
  // Called to release resources. Implementation should call
  // Client::OnResourceReleased when resource is released on its side.
  virtual void ReleaseResource(Resource resource) = 0;

 protected:
  CastResource() : client_(nullptr) {}
  virtual ~CastResource() {}

  void NotifyResourceAcquired();
  void NotifyResourceReleased(Resource remain);

 private:
  Client* client_;

  DISALLOW_COPY_AND_ASSIGN(CastResource);
};

}  // namespace chromecast

#endif
