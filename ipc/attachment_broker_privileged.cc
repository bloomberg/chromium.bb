// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_privileged.h"

#include <algorithm>

#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "ipc/ipc_endpoint.h"

#if defined(OS_WIN)
#include "ipc/attachment_broker_privileged_win.h"
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include <mach/mach.h>

#include "base/process/port_provider_mac.h"
#include "ipc/attachment_broker_privileged_mac.h"
#endif

namespace IPC {

namespace {

#if defined(OS_MACOSX) && !defined(OS_IOS)

// A fake port provider that does nothing. Intended for single process unit
// tests.
class FakePortProvider : public base::PortProvider {
  mach_port_t TaskForPid(base::ProcessHandle process) const override {
    DCHECK_EQ(process, getpid());
    return mach_task_self();
  }
};

base::LazyInstance<FakePortProvider>::Leaky
    g_fake_port_provider = LAZY_INSTANCE_INITIALIZER;

// Passed as a constructor parameter to AttachmentBrokerPrivilegedMac.
base::PortProvider* g_port_provider = nullptr;
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

// On platforms that support attachment brokering, returns a new instance of
// a platform-specific attachment broker. Otherwise returns |nullptr|.
// The caller takes ownership of the newly created instance, and is
// responsible for ensuring that the attachment broker lives longer than
// every IPC::Channel. The new instance automatically registers itself as the
// global attachment broker.
scoped_ptr<AttachmentBrokerPrivileged> CreateBroker() {
#if defined(OS_WIN)
  return scoped_ptr<AttachmentBrokerPrivileged>(
      new IPC::AttachmentBrokerPrivilegedWin);
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  return scoped_ptr<AttachmentBrokerPrivileged>(
      new IPC::AttachmentBrokerPrivilegedMac(g_port_provider));
#else
  return nullptr;
#endif
}

// This class is wrapped in a LazyInstance to ensure that its constructor is
// only called once. The constructor creates an attachment broker and sets it as
// the global broker.
class AttachmentBrokerMakeOnce {
 public:
  AttachmentBrokerMakeOnce() {
    attachment_broker_.reset(CreateBroker().release());
  }

 private:
  scoped_ptr<IPC::AttachmentBrokerPrivileged> attachment_broker_;
};

base::LazyInstance<AttachmentBrokerMakeOnce>::Leaky
    g_attachment_broker_make_once = LAZY_INSTANCE_INITIALIZER;

}  // namespace

AttachmentBrokerPrivileged::AttachmentBrokerPrivileged() {
  IPC::AttachmentBroker::SetGlobal(this);
}

AttachmentBrokerPrivileged::~AttachmentBrokerPrivileged() {
  IPC::AttachmentBroker::SetGlobal(nullptr);
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
// static
void AttachmentBrokerPrivileged::CreateBrokerIfNeeded(
    base::PortProvider* provider) {
  g_port_provider = provider;
  g_attachment_broker_make_once.Get();
}
#else
// static
void AttachmentBrokerPrivileged::CreateBrokerIfNeeded() {
  g_attachment_broker_make_once.Get();
}
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

// static
void AttachmentBrokerPrivileged::CreateBrokerForSingleProcessTests() {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  CreateBrokerIfNeeded(&g_fake_port_provider.Get());
#else
  CreateBrokerIfNeeded();
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)
}

void AttachmentBrokerPrivileged::RegisterCommunicationChannel(
    Endpoint* endpoint) {
  base::AutoLock auto_lock(*get_lock());
  endpoint->SetAttachmentBrokerEndpoint(true);
  auto it = std::find(endpoints_.begin(), endpoints_.end(), endpoint);
  DCHECK(endpoints_.end() == it);
  endpoints_.push_back(endpoint);
}

void AttachmentBrokerPrivileged::DeregisterCommunicationChannel(
    Endpoint* endpoint) {
  base::AutoLock auto_lock(*get_lock());
  auto it = std::find(endpoints_.begin(), endpoints_.end(), endpoint);
  if (it != endpoints_.end())
    endpoints_.erase(it);
}

Sender* AttachmentBrokerPrivileged::GetSenderWithProcessId(base::ProcessId id) {
  get_lock()->AssertAcquired();
  auto it = std::find_if(endpoints_.begin(), endpoints_.end(),
                         [id](Endpoint* c) { return c->GetPeerPID() == id; });
  if (it == endpoints_.end())
    return nullptr;
  return *it;
}

void AttachmentBrokerPrivileged::LogError(UMAError error) {
  UMA_HISTOGRAM_ENUMERATION(
      "IPC.AttachmentBrokerPrivileged.BrokerAttachmentError", error, ERROR_MAX);
}

}  // namespace IPC
