// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_WEB_CONTENTS_H_
#define HEADLESS_PUBLIC_HEADLESS_WEB_CONTENTS_H_

#include <list>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/process/kill.h"
#include "headless/public/headless_export.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace headless {
class HeadlessBrowserContextImpl;
class HeadlessBrowserImpl;
class HeadlessDevToolsTarget;
class HeadlessTabSocket;

// Class representing contents of a browser tab. Should be accessed from browser
// main thread.
class HEADLESS_EXPORT HeadlessWebContents {
 public:
  class HEADLESS_EXPORT Builder;

  virtual ~HeadlessWebContents() {}

  class HEADLESS_EXPORT Observer {
   public:
    // All the following notifications will be called on browser main thread.

    // Indicates that this HeadlessWebContents instance is now ready to be
    // inspected using a HeadlessDevToolsClient.
    //
    // TODO(altimin): Support this event for pages that aren't created by us.
    virtual void DevToolsTargetReady() {}

    // Indicates that a DevTools client attached to this HeadlessWebContents
    // instance.
    virtual void DevToolsClientAttached() {}

    // Indicates that a DevTools client detached from this HeadlessWebContents
    // instance.
    virtual void DevToolsClientDetached() {}

    // This method is invoked when the process of the observed RenderProcessHost
    // exits (either normally or with a crash). To determine if the process
    // closed normally or crashed, examine the |status| parameter.
    //
    // If |status| is TERMINATION_STATUS_LAUNCH_FAILED then |exit_code| will
    // contain a platform specific launch failure error code. Otherwise, it will
    // contain the exit code for the process.
    virtual void RenderProcessExited(base::TerminationStatus status,
                                     int exit_code) {}

   protected:
    Observer() {}
    virtual ~Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Add or remove an observer to receive events from this WebContents.
  // |observer| must outlive this class or be removed prior to being destroyed.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Return a DevTools target corresponding to this tab. Note that this method
  // won't return a valid value until Observer::DevToolsTargetReady has been
  // signaled.
  virtual HeadlessDevToolsTarget* GetDevToolsTarget() = 0;

  // Close this page. |HeadlessWebContents| object will be destroyed.
  virtual void Close() = 0;

  // Returns the headless tab socket for JS -> C++ if one was created.
  virtual HeadlessTabSocket* GetHeadlessTabSocket() const = 0;

  // Returns the devtools frame id corresponding to the |frame_tree_node_id|, if
  // any. Note this relies on an IPC sent from blink during navigation.
  virtual std::string GetUntrustedDevToolsFrameIdForFrameTreeNodeId(
      int process_id,
      int frame_tree_node_id) const = 0;

  virtual int GetMainFrameRenderProcessId() const = 0;

 private:
  friend class HeadlessWebContentsImpl;
  HeadlessWebContents() {}

  DISALLOW_COPY_AND_ASSIGN(HeadlessWebContents);
};

class HEADLESS_EXPORT HeadlessWebContents::Builder {
 public:
  ~Builder();
  Builder(Builder&&);

  // Set an initial URL to ensure that the renderer gets initialized and
  // eventually becomes ready to be inspected. See
  // HeadlessWebContents::Observer::DevToolsTargetReady. The default URL is
  // about:blank.
  Builder& SetInitialURL(const GURL& initial_url);

  // Specify the initial window size (default is configured in browser options).
  Builder& SetWindowSize(const gfx::Size& size);

  enum class TabSocketType {
    NONE,           // No TabSocket binds created (default).
    MAIN_WORLD,     // TabSocket bindings available only to the main world.
    ISOLATED_WORLD  // TabSocket bindings available only to isolated worlds
                    // created via DevTools protocol.
  };

  // Sets the type of TabSocket to be created, if any.
  Builder& SetTabSocketType(TabSocketType type);

  // The returned object is owned by HeadlessBrowser. Call
  // HeadlessWebContents::Close() to dispose it.
  HeadlessWebContents* Build();

 private:
  friend class HeadlessBrowserImpl;
  friend class HeadlessBrowserContextImpl;
  friend class HeadlessWebContentsImpl;

  explicit Builder(HeadlessBrowserContextImpl* browser_context);

  template <typename Interface>
  static void ForwardToServiceFactory(
      const base::Callback<void(mojo::InterfaceRequest<Interface>)>&
          service_factory,
      mojo::ScopedMessagePipeHandle handle) {
    service_factory.Run(mojo::InterfaceRequest<Interface>(std::move(handle)));
  }

  struct MojoService {
    MojoService();
    MojoService(const std::string& service_name,
                const base::Callback<void(mojo::ScopedMessagePipeHandle)>&
                    service_factory);
    ~MojoService();

    std::string service_name;
    base::Callback<void(mojo::ScopedMessagePipeHandle)> service_factory;

   private:
    DISALLOW_COPY_AND_ASSIGN(MojoService);
  };

  HeadlessBrowserContextImpl* browser_context_;

  GURL initial_url_ = GURL("about:blank");
  gfx::Size window_size_;
  std::list<MojoService> mojo_services_;
  TabSocketType tab_socket_type_ = TabSocketType::NONE;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_WEB_CONTENTS_H_
