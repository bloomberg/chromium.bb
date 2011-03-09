// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_H_
#pragma once

#include <map>
#include <queue>
#include <string>

#include "app/surface/transport_dib.h"
#include "base/platform_file.h"
#include "base/process.h"
#include "base/scoped_callback_factory.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"

class CommandLine;
class RendererMainThread;
class RenderWidgetHelper;
class VisitedLinkUpdater;

namespace base {
class SharedMemory;
}

// Implements a concrete RenderProcessHost for the browser process for talking
// to actual renderer processes (as opposed to mocks).
//
// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.
//
// This object is refcounted so that it can release its resources when all
// hosts using it go away.
//
// This object communicates back and forth with the RenderProcess object
// running in the renderer process. Each RenderProcessHost and RenderProcess
// keeps a list of RenderView (renderer) and TabContents (browser) which
// are correlated with IDs. This way, the Views and the corresponding ViewHosts
// communicate through the two process objects.
class BrowserRenderProcessHost : public RenderProcessHost,
                                 public NotificationObserver,
                                 public ChildProcessLauncher::Client {
 public:
  explicit BrowserRenderProcessHost(Profile* profile);
  ~BrowserRenderProcessHost();

  // RenderProcessHost implementation (public portion).
  virtual bool Init(bool is_accessibility_enabled, bool is_extensions_process);
  virtual int GetNextRoutingID();
  virtual void CancelResourceRequests(int render_widget_id);
  virtual void CrossSiteClosePageACK(const ViewMsg_ClosePage_Params& params);
  virtual bool WaitForUpdateMsg(int render_widget_id,
                                const base::TimeDelta& max_delay,
                                IPC::Message* msg);
  virtual void ReceivedBadMessage();
  virtual void WidgetRestored();
  virtual void WidgetHidden();
  virtual void ViewCreated();
  virtual void SendVisitedLinkTable(base::SharedMemory* table_memory);
  virtual void AddVisitedLinks(const VisitedLinkCommon::Fingerprints& links);
  virtual void ResetVisitedLinks();
  virtual bool FastShutdownIfPossible();
  virtual bool SendWithTimeout(IPC::Message* msg, int timeout_ms);
  virtual base::ProcessHandle GetHandle();
  virtual TransportDIB* GetTransportDIB(TransportDIB::Id dib_id);

  // IPC::Channel::Sender via RenderProcessHost.
  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener via RenderProcessHost.
  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // ChildProcessLauncher::Client implementation.
  virtual void OnProcessLaunched();

 private:
  friend class VisitRelayingRenderProcessHost;

  // Creates and adds the IO thread message filters.
  void CreateMessageFilters();

  // Control message handlers.
  void OnUpdatedCacheStats(const WebKit::WebCache::UsageStats& stats);
  void SuddenTerminationChanged(bool enabled);
  void OnExtensionAddListener(const std::string& extension_id,
                              const std::string& event_name);
  void OnExtensionRemoveListener(const std::string& extension_id,
                                 const std::string& event_name);
  void OnExtensionCloseChannel(int port_id);
  void OnUserMetricsRecordAction(const std::string& action);

  // Initialize support for visited links. Send the renderer process its initial
  // set of visited links.
  void InitVisitedLinks();

  // Initialize support for user scripts. Send the renderer process its initial
  // set of scripts and listen for updates to scripts.
  void InitUserScripts();

  // Initialize support for extension APIs. Send the list of registered API
  // functions to thre renderer process.
  void InitExtensions();

  // Sends the renderer process a new set of user scripts.
  void SendUserScriptsUpdate(base::SharedMemory* shared_memory);

  // Generates a command line to be used to spawn a renderer and appends the
  // results to |*command_line|.
  void AppendRendererCommandLine(CommandLine* command_line) const;

  // Copies applicable command line switches from the given |browser_cmd| line
  // flags to the output |renderer_cmd| line flags. Not all switches will be
  // copied over.
  void PropagateBrowserCommandLineToRenderer(const CommandLine& browser_cmd,
                                             CommandLine* renderer_cmd) const;

  // Callers can reduce the RenderProcess' priority.
  void SetBackgrounded(bool backgrounded);

  // The renderer has requested that we initialize its spellchecker. This should
  // generally only be called once per session, as after the first call, all
  // future renderers will be passed the initialization information on startup
  // (or when the dictionary changes in some way).
  void OnSpellCheckerRequestDictionary();

  // Tell the renderer of a new word that has been added to the custom
  // dictionary.
  void AddSpellCheckWord(const std::string& word);

  // Pass the renderer some basic intialization information. Note that the
  // renderer will not load Hunspell until it needs to.
  void InitSpellChecker();

  // Tell the renderer that auto spell correction has been enabled/disabled.
  void EnableAutoSpellCorrect(bool enable);

  // Initializes client-side phishing detection.  Starts reading the phishing
  // model from the client-side detection service class.  Once the model is read
  // OpenPhishingModelDone() is invoked.
  void InitClientSidePhishingDetection();

  // Called once the client-side detection service class is done with opening
  // the model file.
  void OpenPhishingModelDone(base::PlatformFile model_file);

  NotificationRegistrar registrar_;

  // The count of currently visible widgets.  Since the host can be a container
  // for multiple widgets, it uses this count to determine when it should be
  // backgrounded.
  int32 visible_widgets_;

  // Does this process have backgrounded priority.
  bool backgrounded_;

  // Used to allow a RenderWidgetHost to intercept various messages on the
  // IO thread.
  scoped_refptr<RenderWidgetHelper> widget_helper_;

  // A map of transport DIB ids to cached TransportDIBs
  std::map<TransportDIB::Id, TransportDIB*> cached_dibs_;
  enum {
    // This is the maximum size of |cached_dibs_|
    MAX_MAPPED_TRANSPORT_DIBS = 3,
  };

  // Map a transport DIB from its Id and return it. Returns NULL on error.
  TransportDIB* MapTransportDIB(TransportDIB::Id dib_id);

  void ClearTransportDIBCache();
  // This is used to clear our cache five seconds after the last use.
  base::DelayTimer<BrowserRenderProcessHost> cached_dibs_cleaner_;

  // Used in single-process mode.
  scoped_ptr<RendererMainThread> in_process_renderer_;

  // Buffer visited links and send them to to renderer.
  scoped_ptr<VisitedLinkUpdater> visited_link_updater_;

  // True if this prcoess should have accessibility enabled;
  bool accessibility_enabled_;

  // True iff this process is being used as an extension process. Not valid
  // when running in single-process mode.
  bool extension_process_;

  // Usedt to launch and terminate the process without blocking the UI thread.
  scoped_ptr<ChildProcessLauncher> child_process_;

  // Messages we queue while waiting for the process handle.  We queue them here
  // instead of in the channel so that we ensure they're sent after init related
  // messages that are sent once the process handle is available.  This is
  // because the queued messages may have dependencies on the init messages.
  std::queue<IPC::Message*> queued_messages_;

  base::ScopedCallbackFactory<BrowserRenderProcessHost> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserRenderProcessHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_H_
