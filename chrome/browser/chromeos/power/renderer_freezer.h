// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_RENDERER_FREEZER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_RENDERER_FREEZER_H_

#include <set>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/time/time.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_process_host_observer.h"

namespace content {
class RenderProcessHost;
}

namespace chromeos {

// Freezes the chrome renderers when the system is about to suspend and thaws
// them after the system fully resumes.  This class registers itself as a
// PowerManagerClient::Observer on creation and unregisters itself on
// destruction.
class CHROMEOS_EXPORT RendererFreezer
    : public PowerManagerClient::Observer,
      public content::NotificationObserver,
      public content::RenderProcessHostObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // If |frozen| is true, marks the renderer process |handle| to be frozen
    // when FreezeRenderers() is called; otherwise marks it to remain unfrozen.
    virtual void SetShouldFreezeRenderer(base::ProcessHandle handle,
                                         bool frozen) = 0;

    // Freezes the renderers marked for freezing by SetShouldFreezeRenderer().
    // Returns true if the operation was successful.
    virtual bool FreezeRenderers() = 0;

    // Thaws the chrome renderers that were frozen by the call to
    // FreezeRenderers().  Returns true if the operation was successful.
    virtual bool ThawRenderers() = 0;

    // Returns true iff the delegate is capable of freezing renderers.
    virtual bool CanFreezeRenderers() = 0;
  };

  explicit RendererFreezer(scoped_ptr<Delegate> delegate);
  ~RendererFreezer() override;

  // PowerManagerClient::Observer implementation.
  void SuspendImminent() override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // content::RenderProcessHostObserver overrides.
  void RenderProcessExited(content::RenderProcessHost* host,
                           base::TerminationStatus status,
                           int exit_code) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

 private:
  // Called whenever a new renderer process is created.
  void OnRenderProcessCreated(content::RenderProcessHost* rph);

  // Called when all asynchronous work is complete and renderers can be frozen.
  void OnReadyToSuspend(const base::Closure& power_manager_callback);

  // Used to ensure that renderers do not get frozen if the suspend is canceled.
  base::CancelableClosure suspend_readiness_callback_;

  // Tracks if the renderers are currently frozen.
  bool frozen_;

  // Delegate that takes care of actually freezing and thawing renderers for us.
  scoped_ptr<Delegate> delegate_;

  // Set that keeps track of the RenderProcessHosts for processes that are
  // hosting GCM extensions.
  std::set<int> gcm_extension_processes_;

  // Manages notification registrations.
  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<RendererFreezer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererFreezer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_RENDERER_FREEZER_H_
