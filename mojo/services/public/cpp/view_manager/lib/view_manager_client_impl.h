// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_CLIENT_IMPL_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_CLIENT_IMPL_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/public/interfaces/window_manager/window_manager.mojom.h"

class SkBitmap;

namespace mojo {
class ApplicationConnection;
class ViewManager;
class ViewManagerDelegate;
class ViewManagerTransaction;

// Manages the connection with the View Manager service.
class ViewManagerClientImpl : public ViewManager,
                              public InterfaceImpl<ViewManagerClient>,
                              public WindowManagerClient {
 public:
  ViewManagerClientImpl(ViewManagerDelegate* delegate,
                        ApplicationConnection* app_connection);
  virtual ~ViewManagerClientImpl();

  bool connected() const { return connected_; }
  ConnectionSpecificId connection_id() const { return connection_id_; }

  // API exposed to the view implementations that pushes local changes to the
  // service.
  Id CreateView();
  void DestroyView(Id view_id);

  // These methods take TransportIds. For views owned by the current connection,
  // the connection id high word can be zero. In all cases, the TransportId 0x1
  // refers to the root view.
  void AddChild(Id child_id, Id parent_id);
  void RemoveChild(Id child_id, Id parent_id);

  void Reorder(Id view_id, Id relative_view_id, OrderDirection direction);

  // Returns true if the specified view was created by this connection.
  bool OwnsView(Id id) const;

  void SetBounds(Id view_id, const gfx::Rect& bounds);
  void SetViewContents(Id view_id, const SkBitmap& contents);
  void SetFocus(Id view_id);
  void SetVisible(Id view_id, bool visible);

  void Embed(const String& url, Id view_id);
  void Embed(const String& url,
             Id view_id,
             ServiceProviderPtr service_provider);

  void set_change_acked_callback(const base::Callback<void(void)>& callback) {
    change_acked_callback_ = callback;
  }
  void ClearChangeAckedCallback() {
    change_acked_callback_ = base::Callback<void(void)>();
  }

  // Start/stop tracking views. While tracked, they can be retrieved via
  // ViewManager::GetViewById.
  void AddView(View* view);
  void RemoveView(Id view_id);

 private:
  friend class RootObserver;

  typedef std::map<Id, View*> IdToViewMap;

  // Overridden from ViewManager:
  virtual void SetWindowManagerDelegate(
      WindowManagerDelegate* delegate) OVERRIDE;
  virtual void DispatchEvent(View* target, EventPtr event) OVERRIDE;
  virtual const std::string& GetEmbedderURL() const OVERRIDE;
  virtual const std::vector<View*>& GetRoots() const OVERRIDE;
  virtual View* GetViewById(Id id) OVERRIDE;

  // Overridden from InterfaceImpl:
  virtual void OnConnectionEstablished() OVERRIDE;

  // Overridden from ViewManagerClient:
  virtual void OnEmbed(ConnectionSpecificId connection_id,
                       const String& creator_url,
                       ViewDataPtr root,
                       InterfaceRequest<ServiceProvider> services) OVERRIDE;
  virtual void OnViewBoundsChanged(Id view_id,
                                   RectPtr old_bounds,
                                   RectPtr new_bounds) OVERRIDE;
  virtual void OnViewHierarchyChanged(Id view_id,
                                      Id new_parent_id,
                                      Id old_parent_id,
                                      Array<ViewDataPtr> views) OVERRIDE;
  virtual void OnViewReordered(Id view_id,
                               Id relative_view_id,
                               OrderDirection direction) OVERRIDE;
  virtual void OnViewDeleted(Id view_id) OVERRIDE;
  virtual void OnViewInputEvent(Id view,
                                EventPtr event,
                                const Callback<void()>& callback) OVERRIDE;
  virtual void Embed(
      const String& url,
      InterfaceRequest<ServiceProvider> service_provider) OVERRIDE;
  virtual void DispatchOnViewInputEvent(EventPtr event) OVERRIDE;

    // Overridden from WindowManagerClient:
  virtual void OnWindowManagerReady() OVERRIDE;
  virtual void OnCaptureChanged(Id old_capture_view_id,
                                Id new_capture_view_id) OVERRIDE;
  virtual void OnFocusChanged(Id old_focused_view_id,
                              Id new_focused_view_id) OVERRIDE;
  virtual void OnActiveWindowChanged(Id old_focused_window,
                                     Id new_focused_window) OVERRIDE;

  void RemoveRoot(View* root);

  void OnActionCompleted(bool success);
  void OnActionCompletedWithErrorCode(ErrorCode code);

  base::Callback<void(bool)> ActionCompletedCallback();
  base::Callback<void(ErrorCode)> ActionCompletedCallbackWithErrorCode();

  bool connected_;
  ConnectionSpecificId connection_id_;
  ConnectionSpecificId next_id_;

  std::string creator_url_;

  base::Callback<void(void)> change_acked_callback_;

  ViewManagerDelegate* delegate_;
  WindowManagerDelegate* window_manager_delegate_;

  std::vector<View*> roots_;

  IdToViewMap views_;

  ViewManagerService* service_;

  WindowManagerServicePtr window_manager_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerClientImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_CLIENT_IMPL_H_
