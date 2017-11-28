// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/exo_parts.h"

#include "base/memory/ptr_util.h"

#if defined(USE_GLIB)
#include <glib.h>
#endif

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/common/chrome_switches.h"
#include "components/exo/display.h"
#include "components/exo/file_helper.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wm_helper.h"
#include "content/public/browser/browser_thread.h"
#include "ui/arc/notification/arc_notification_surface_manager_impl.h"

namespace {

constexpr char kMimeTypeArcUriList[] = "application/x-arc-uri-list";

class ChromeFileHelper : public exo::FileHelper {
 public:
  ChromeFileHelper() {}
  ~ChromeFileHelper() override {}

  // exo::FileHelper:
  std::string GetMimeTypeForUriList() const override {
    return kMimeTypeArcUriList;
  }
  bool ConvertPathToUrl(const base::FilePath& path, GURL* out) override {
    return file_manager::util::ConvertPathToArcUrl(path, out);
  }
};

}  // namespace

#if defined(USE_GLIB)
namespace {

struct GLibWaylandSource : public GSource {
  // Note: The GLibWaylandSource is created and destroyed by GLib. So its
  // constructor/destructor may or may not get called.
  exo::wayland::Server* server;
  GPollFD* poll_fd;
};

gboolean WaylandSourcePrepare(GSource* source, gint* timeout_ms) {
  *timeout_ms = -1;
  return FALSE;
}

gboolean WaylandSourceCheck(GSource* source) {
  GLibWaylandSource* wayland_source = static_cast<GLibWaylandSource*>(source);
  return (wayland_source->poll_fd->revents & G_IO_IN) ? TRUE : FALSE;
}

gboolean WaylandSourceDispatch(GSource* source,
                               GSourceFunc unused_func,
                               gpointer data) {
  GLibWaylandSource* wayland_source = static_cast<GLibWaylandSource*>(source);
  wayland_source->server->Dispatch(base::TimeDelta());
  wayland_source->server->Flush();
  return TRUE;
}

GSourceFuncs g_wayland_source_funcs = {WaylandSourcePrepare, WaylandSourceCheck,
                                       WaylandSourceDispatch, nullptr};

}  // namespace

class ExoParts::WaylandWatcher {
 public:
  explicit WaylandWatcher(exo::wayland::Server* server)
      : wayland_poll_(new GPollFD),
        wayland_source_(static_cast<GLibWaylandSource*>(
            g_source_new(&g_wayland_source_funcs, sizeof(GLibWaylandSource)))) {
    wayland_poll_->fd = server->GetFileDescriptor();
    wayland_poll_->events = G_IO_IN;
    wayland_poll_->revents = 0;
    wayland_source_->server = server;
    wayland_source_->poll_fd = wayland_poll_.get();
    g_source_add_poll(wayland_source_, wayland_poll_.get());
    g_source_set_can_recurse(wayland_source_, TRUE);
    g_source_set_callback(wayland_source_, nullptr, nullptr, nullptr);
    g_source_attach(wayland_source_, g_main_context_default());
  }
  ~WaylandWatcher() {
    g_source_destroy(wayland_source_);
    g_source_unref(wayland_source_);
  }

 private:
  // The poll attached to |wayland_source_|.
  std::unique_ptr<GPollFD> wayland_poll_;

  // The GLib event source for wayland events.
  GLibWaylandSource* wayland_source_;

  DISALLOW_COPY_AND_ASSIGN(WaylandWatcher);
};
#else
class ExoParts::WaylandWatcher : public base::MessagePumpLibevent::Watcher {
 public:
  explicit WaylandWatcher(exo::wayland::Server* server)
      : controller_(FROM_HERE), server_(server) {
    base::MessageLoopForUI::current()->WatchFileDescriptor(
        server_->GetFileDescriptor(),
        true,  // persistent
        base::MessagePumpLibevent::WATCH_READ, &controller_, this);
  }

  // base::MessagePumpLibevent::Watcher:
  void OnFileCanReadWithoutBlocking(int fd) override {
    server_->Dispatch(base::TimeDelta());
    server_->Flush();
  }
  void OnFileCanWriteWithoutBlocking(int fd) override { NOTREACHED(); }

 private:
  base::MessagePumpLibevent::FileDescriptorWatcher controller_;
  exo::wayland::Server* const server_;

  DISALLOW_COPY_AND_ASSIGN(WaylandWatcher);
};
#endif

// static
std::unique_ptr<ExoParts> ExoParts::CreateIfNecessary() {
  // For mash, exosphere will not run in the browser process.
  if (ash_util::IsRunningInMash())
    return nullptr;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWaylandServer)) {
    return nullptr;
  }

  return base::WrapUnique(new ExoParts());
}

ExoParts::~ExoParts() {
  wayland_watcher_.reset();
  wayland_server_.reset();
  display_.reset();
  exo::WMHelper::SetInstance(nullptr);
  wm_helper_.reset();
}

ExoParts::ExoParts() {
  arc_notification_surface_manager_ =
      base::MakeUnique<arc::ArcNotificationSurfaceManagerImpl>();
  DCHECK(!ash_util::IsRunningInMash());
  wm_helper_ = base::MakeUnique<exo::WMHelper>();
  exo::WMHelper::SetInstance(wm_helper_.get());
  display_ =
      base::MakeUnique<exo::Display>(arc_notification_surface_manager_.get(),
                                     base::MakeUnique<ChromeFileHelper>());
  wayland_server_ = exo::wayland::Server::Create(display_.get());
  // Wayland server creation can fail if XDG_RUNTIME_DIR is not set correctly.
  if (wayland_server_)
    wayland_watcher_ = base::MakeUnique<WaylandWatcher>(wayland_server_.get());
}
