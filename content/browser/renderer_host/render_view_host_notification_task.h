// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines utility functions for sending notifications (calling
// methods that return void and do not have out params) to the RenderViewHost
// or one of its delegate interfaces.  The notifications are dispatched
// asynchronously, and only if the specified RenderViewHost still exists.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_NOTIFICATION_TASK_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_NOTIFICATION_TASK_H_
#pragma once

#include "base/callback.h"
#include "base/task.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"

// ----------------------------------------------------------------------------

namespace internal {

// The simplest Mapper, used when proxying calls to a RenderViewHost.
class RenderViewHostIdentityMapper {
 public:
  typedef RenderViewHost MappedType;
  static MappedType* Map(RenderViewHost* rvh) { return rvh; }
};

template <typename Method, typename Params,
          typename Mapper = RenderViewHostIdentityMapper>
class RenderViewHostNotificationTask : public Task {
 public:
  RenderViewHostNotificationTask(int render_process_id,
                                 int render_view_id,
                                 Method method,
                                 const Params& params)
      : render_process_id_(render_process_id),
        render_view_id_(render_view_id),
        unbound_method_(method, params) {
  }

  virtual void Run() {
    RenderViewHost* rvh = RenderViewHost::FromID(render_process_id_,
                                                 render_view_id_);
    typename Mapper::MappedType* obj = Mapper::Map(rvh);
    if (obj)
      unbound_method_.Run(obj);
  }

 private:
  int render_process_id_;
  int render_view_id_;
  UnboundMethod<typename Mapper::MappedType, Method, Params> unbound_method_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostNotificationTask);
};

// For proxying calls to RenderViewHost

template <typename Method, typename Params>
inline void CallRenderViewHostHelper(int render_process_id, int render_view_id,
                                     Method method, const Params& params) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      new RenderViewHostNotificationTask<Method, Params>(render_process_id,
                                                         render_view_id,
                                                         method,
                                                         params));
}

// For proxying calls to RenderViewHostDelegate::ContentSettings

class RenderViewHostToContentSettingsDelegate {
 public:
  typedef RenderViewHostDelegate::ContentSettings MappedType;
  static MappedType* Map(RenderViewHost* rvh) {
    return rvh ? rvh->delegate()->GetContentSettingsDelegate() : NULL;
  }
};

template <typename Method, typename Params>
inline void CallRenderViewHostContentSettingsDelegateHelper(
    int render_process_id,
    int render_view_id,
    Method method,
    const Params& params) {

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      new RenderViewHostNotificationTask<
          Method, Params, RenderViewHostToContentSettingsDelegate>(
              render_process_id,
              render_view_id,
              method,
              params));
}

// For proxying calls to RenderViewHostDelegate::RendererManagement

class RenderViewHostToRendererManagementDelegate {
 public:
  typedef RenderViewHostDelegate::RendererManagement MappedType;
  static MappedType* Map(RenderViewHost* rvh) {
    return rvh ? rvh->delegate()->GetRendererManagementDelegate() : NULL;
  }
};

template <typename Method, typename Params>
inline void CallRenderViewHostRendererManagementDelegateHelper(
    int render_process_id,
    int render_view_id,
    Method method,
    const Params& params) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      new RenderViewHostNotificationTask<
          Method, Params, RenderViewHostToRendererManagementDelegate>(
              render_process_id,
              render_view_id,
              method,
              params));
}

// For proxying calls to RenderViewHostDelegate::SSL

class RenderViewHostToSSLDelegate {
 public:
  typedef RenderViewHostDelegate::SSL MappedType;
  static MappedType* Map(RenderViewHost* rvh) {
    return rvh ? rvh->delegate()->GetSSLDelegate() : NULL;
  }
};

template <typename Method, typename Params>
inline void CallRenderViewHostSSLDelegateHelper(
    int render_process_id,
    int render_view_id,
    Method method,
    const Params& params) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      new RenderViewHostNotificationTask<
          Method, Params, RenderViewHostToSSLDelegate>(
              render_process_id,
              render_view_id,
              method,
              params));
}

}  // namespace internal

// ----------------------------------------------------------------------------
// Proxy calls to the specified RenderViewHost.

template <typename Method>
inline void CallRenderViewHost(int render_process_id,
                               int render_view_id,
                               Method method) {
  internal::CallRenderViewHostHelper(render_process_id,
                                     render_view_id,
                                     method,
                                     MakeTuple());
}

template <typename Method, typename A>
inline void CallRenderViewHost(int render_process_id,
                               int render_view_id,
                               Method method,
                               const A& a) {
  internal::CallRenderViewHostHelper(render_process_id,
                                     render_view_id,
                                     method,
                                     MakeTuple(a));
}

template <typename Method, typename A, typename B>
inline void CallRenderViewHost(int render_process_id,
                               int render_view_id,
                               Method method,
                               const A& a,
                               const B& b) {
  internal::CallRenderViewHostHelper(render_process_id,
                                     render_view_id,
                                     method,
                                     MakeTuple(a, b));
}

// ----------------------------------------------------------------------------
// Proxy calls to the specified RenderViewHost's ContentSettings delegate.

template <typename Method>
inline void CallRenderViewHostContentSettingsDelegate(int render_process_id,
                                                      int render_view_id,
                                                      Method method) {
  internal::CallRenderViewHostContentSettingsDelegateHelper(render_process_id,
                                                            render_view_id,
                                                            method,
                                                            MakeTuple());
}

template <typename Method, typename A>
inline void CallRenderViewHostContentSettingsDelegate(int render_process_id,
                                                      int render_view_id,
                                                      Method method,
                                                      const A& a) {
  internal::CallRenderViewHostContentSettingsDelegateHelper(render_process_id,
                                                            render_view_id,
                                                            method,
                                                            MakeTuple(a));
        }

template <typename Method, typename A, typename B>
inline void CallRenderViewHostContentSettingsDelegate(int render_process_id,
                                                      int render_view_id,
                                                      Method method,
                                                      const A& a,
                                                      const B& b) {
  internal::CallRenderViewHostContentSettingsDelegateHelper(render_process_id,
                                                            render_view_id,
                                                            method,
                                                            MakeTuple(a, b));
}

template <typename Method, typename A, typename B, typename C>
inline void CallRenderViewHostContentSettingsDelegate(int render_process_id,
                                                      int render_view_id,
                                                      Method method,
                                                      const A& a,
                                                      const B& b,
                                                      const C& c) {
  internal::CallRenderViewHostContentSettingsDelegateHelper(render_process_id,
                                                            render_view_id,
                                                            method,
                                                            MakeTuple(a, b, c));
}

template <typename Method, typename A, typename B, typename C, typename D>
inline void CallRenderViewHostContentSettingsDelegate(int render_process_id,
                                                      int render_view_id,
                                                      Method method,
                                                      const A& a,
                                                      const B& b,
                                                      const C& c,
                                                      const D& d) {
  internal::CallRenderViewHostContentSettingsDelegateHelper(
      render_process_id,
      render_view_id,
      method,
      MakeTuple(a, b, c, d));
}

template <typename Method,
          typename A, typename B, typename C, typename D, typename E>
inline void CallRenderViewHostContentSettingsDelegate(int render_process_id,
                                                      int render_view_id,
                                                      Method method,
                                                      const A& a,
                                                      const B& b,
                                                      const C& c,
                                                      const D& d,
                                                      const E& e) {
  internal::CallRenderViewHostContentSettingsDelegateHelper(
      render_process_id, render_view_id, method, MakeTuple(a, b, c, d, e));
}

// ----------------------------------------------------------------------------
// Proxy calls to the specified RenderViewHost's RendererManagement delegate.

template <typename Method>
inline void CallRenderViewHostRendererManagementDelegate(int render_process_id,
                                                         int render_view_id,
                                                         Method method) {
  internal::CallRenderViewHostRendererManagementDelegateHelper(
      render_process_id,
      render_view_id,
      method,
      MakeTuple());
}

template <typename Method, typename A>
inline void CallRenderViewHostRendererManagementDelegate(int render_process_id,
                                                         int render_view_id,
                                                         Method method,
                                                         const A& a) {
  internal::CallRenderViewHostRendererManagementDelegateHelper(
      render_process_id,
      render_view_id,
      method,
      MakeTuple(a));
}

template <typename Method, typename A, typename B>
inline void CallRenderViewHostRendererManagementDelegate(int render_process_id,
                                                         int render_view_id,
                                                         Method method,
                                                         const A& a,
                                                         const B& b) {
  internal::CallRenderViewHostRendererManagementDelegateHelper(
      render_process_id,
      render_view_id,
      method,
      MakeTuple(a, b));
}

// ----------------------------------------------------------------------------
// Proxy calls to the specified RenderViewHost's SSL delegate.

template <typename Method, typename A>
inline void CallRenderViewHostSSLDelegate(int render_process_id,
                                          int render_view_id,
                                          Method method,
                                          const A& a) {
  internal::CallRenderViewHostSSLDelegateHelper(
      render_process_id,
      render_view_id,
      method,
      MakeTuple(a));
}

template <typename Method, typename A, typename B>
inline void CallRenderViewHostSSLDelegate(int render_process_id,
                                          int render_view_id,
                                          Method method,
                                          const A& a,
                                          const B& b) {
  internal::CallRenderViewHostSSLDelegateHelper(
      render_process_id,
      render_view_id,
      method,
      MakeTuple(a, b));
}

// ----------------------------------------------------------------------------

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_NOTIFICATION_TASK_H_
