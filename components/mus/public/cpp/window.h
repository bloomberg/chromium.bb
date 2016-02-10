// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_H_

#include <stdint.h>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/mus/common/types.h"
#include "components/mus/public/interfaces/mus_constants.mojom.h"
#include "components/mus/public/interfaces/surface_id.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/shell/public/interfaces/interface_provider.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Size;
}

namespace mus {

class InputEventHandler;
class ServiceProviderImpl;
class WindowObserver;
class WindowSurface;
class WindowSurfaceBinding;
class WindowTreeClientImpl;
class WindowTreeConnection;

namespace {
class OrderChangedNotifier;
}

// Defined in window_property.h (which we do not include)
template <typename T>
struct WindowProperty;

// Windows are owned by the WindowTreeConnection. See WindowTreeDelegate for
// details on ownership.
//
// TODO(beng): Right now, you'll have to implement a WindowObserver to track
//             destruction and NULL any pointers you have.
//             Investigate some kind of smart pointer or weak pointer for these.
class Window {
 public:
  using Children = std::vector<Window*>;
  using EmbedCallback = base::Callback<void(bool, ConnectionSpecificId)>;
  using PropertyDeallocator = void (*)(int64_t value);
  using SharedProperties = std::map<std::string, std::vector<uint8_t>>;

  // Destroys this window and all its children. Destruction is allowed for
  // windows that were created by this connection, or the roots. For windows
  // from other connections (except the roots), Destroy() does nothing. If the
  // destruction is allowed observers are notified and the Window is
  // immediately deleted.
  void Destroy();

  WindowTreeConnection* connection() { return connection_; }

  // Configuration.
  Id id() const { return id_; }

  // Geometric disposition relative to parent window.
  const gfx::Rect& bounds() const { return bounds_; }
  void SetBounds(const gfx::Rect& bounds);

  // Geometric disposition relative to root window.
  gfx::Rect GetBoundsInRoot() const;

  const gfx::Insets& client_area() const { return client_area_; }
  const std::vector<gfx::Rect>& additional_client_areas() {
    return additional_client_areas_;
  }
  void SetClientArea(const gfx::Insets& new_client_area) {
    SetClientArea(new_client_area, std::vector<gfx::Rect>());
  }
  void SetClientArea(const gfx::Insets& new_client_area,
                     const std::vector<gfx::Rect>& additional_client_areas);

  // Visibility (also see IsDrawn()). When created windows are hidden.
  bool visible() const { return visible_; }
  void SetVisible(bool value);

  // Cursors
  mojom::Cursor predefined_cursor() const { return cursor_id_; }
  void SetPredefinedCursor(mus::mojom::Cursor cursor_id);

  // A Window is drawn if the Window and all its ancestors are visible and the
  // Window is attached to the root.
  bool IsDrawn() const;

  const mojom::ViewportMetrics& viewport_metrics() {
    return *viewport_metrics_;
  }

  scoped_ptr<WindowSurface> RequestSurface(mojom::SurfaceType type);

  void AttachSurface(mojom::SurfaceType type,
                     scoped_ptr<WindowSurfaceBinding> surface_binding);

  // The template-ized versions of the following methods rely on the presence
  // of a mojo::TypeConverter<const std::vector<uint8_t>, T>.
  // Sets a shared property on the window which is sent to the window server and
  // shared with other clients that can view this window.
  template <typename T>
  void SetSharedProperty(const std::string& name, const T& data);
  // Gets a shared property set on the window. The property must exist. Call
  // HasSharedProperty() before calling.
  template <typename T>
  T GetSharedProperty(const std::string& name) const;
  // Removes the shared property.
  void ClearSharedProperty(const std::string& name);
  bool HasSharedProperty(const std::string& name) const;

  // TODO(beng): Test only, should move to a helper.
  const SharedProperties& shared_properties() { return properties_; }

  // Sets the |value| of the given window |property|. Setting to the default
  // value (e.g., NULL) removes the property. The caller is responsible for the
  // lifetime of any object set as a property on the Window.
  //
  // These properties are not visible to the window server.
  template <typename T>
  void SetLocalProperty(const WindowProperty<T>* property, T value);

  // Returns the value of the given window |property|.  Returns the
  // property-specific default value if the property was not previously set.
  //
  // These properties are only visible in the current process and are not
  // shared with other mojo services.
  template <typename T>
  T GetLocalProperty(const WindowProperty<T>* property) const;

  // Sets the |property| to its default value. Useful for avoiding a cast when
  // setting to NULL.
  //
  // These properties are only visible in the current process and are not
  // shared with other mojo services.
  template <typename T>
  void ClearLocalProperty(const WindowProperty<T>* property);

  void set_input_event_handler(InputEventHandler* input_event_handler) {
    input_event_handler_ = input_event_handler;
  }

  // Observation.
  void AddObserver(WindowObserver* observer);
  void RemoveObserver(WindowObserver* observer);

  // Tree.
  Window* parent() { return parent_; }
  const Window* parent() const { return parent_; }

  Window* GetRoot() {
    return const_cast<Window*>(const_cast<const Window*>(this)->GetRoot());
  }
  const Window* GetRoot() const;

  void AddChild(Window* child);
  void RemoveChild(Window* child);
  const Children& children() const { return children_; }

  void Reorder(Window* relative, mojom::OrderDirection direction);
  void MoveToFront();
  void MoveToBack();

  // Returns true if |child| is this or a descendant of this.
  bool Contains(Window* child) const;

  void AddTransientWindow(Window* transient_window);
  void RemoveTransientWindow(Window* transient_window);

  // TODO(fsamuel): Figure out if we want to refactor transient window
  // management into a separate class.
  // Transient tree.
  Window* transient_parent() { return transient_parent_; }
  const Window* transient_parent() const { return transient_parent_; }
  const Children& transient_children() const { return transient_children_; }

  Window* GetChildById(Id id);

  void SetTextInputState(mojo::TextInputStatePtr state);
  void SetImeVisibility(bool visible, mojo::TextInputStatePtr state);

  // Focus.
  void SetFocus();
  bool HasFocus() const;
  void SetCanFocus(bool can_focus);

  // Embedding. See window_tree.mojom for details.
  void Embed(mus::mojom::WindowTreeClientPtr client);

  // NOTE: callback is run synchronously if Embed() is not allowed on this
  // Window.
  void Embed(mus::mojom::WindowTreeClientPtr client,
             uint32_t policy_bitmask,
             const EmbedCallback& callback);

  // TODO(sky): this API is only applicable to the WindowManager. Move it
  // to a better place.
  void RequestClose();

 protected:
  // This class is subclassed only by test classes that provide a public ctor.
  Window();
  ~Window();

 private:
  friend class WindowPrivate;
  friend class WindowTreeClientImpl;

  Window(WindowTreeConnection* connection, Id id);

  WindowTreeClientImpl* tree_client();

  // Applies a shared property change locally and forwards to the server. If
  // |data| is null, this property is deleted.
  void SetSharedPropertyInternal(const std::string& name,
                                 const std::vector<uint8_t>* data);
  // Called by the public {Set,Get,Clear}Property functions.
  int64_t SetLocalPropertyInternal(const void* key,
                                   const char* name,
                                   PropertyDeallocator deallocator,
                                   int64_t value,
                                   int64_t default_value);
  int64_t GetLocalPropertyInternal(const void* key,
                                   int64_t default_value) const;

  void LocalDestroy();
  void LocalAddChild(Window* child);
  void LocalRemoveChild(Window* child);
  void LocalAddTransientWindow(Window* transient_window);
  void LocalRemoveTransientWindow(Window* transient_window);
  // Returns true if the order actually changed.
  bool LocalReorder(Window* relative, mojom::OrderDirection direction);
  void LocalSetBounds(const gfx::Rect& old_bounds, const gfx::Rect& new_bounds);
  void LocalSetClientArea(
      const gfx::Insets& new_client_area,
      const std::vector<gfx::Rect>& additional_client_areas);
  void LocalSetViewportMetrics(const mojom::ViewportMetrics& old_metrics,
                               const mojom::ViewportMetrics& new_metrics);
  void LocalSetDrawn(bool drawn);
  void LocalSetVisible(bool visible);
  void LocalSetPredefinedCursor(mojom::Cursor cursor_id);
  void LocalSetSharedProperty(const std::string& name,
                              const std::vector<uint8_t>* data);

  // Notifies this winodw that its stacking position has changed.
  void NotifyWindowStackingChanged();
  // Methods implementing visibility change notifications. See WindowObserver
  // for more details.
  void NotifyWindowVisibilityChanged(Window* target);
  // Notifies this window's observers. Returns false if |this| was deleted
  // during the call (by an observer), otherwise true.
  bool NotifyWindowVisibilityChangedAtReceiver(Window* target);
  // Notifies this window and its child hierarchy. Returns false if |this| was
  // deleted during the call (by an observer), otherwise true.
  bool NotifyWindowVisibilityChangedDown(Window* target);
  // Notifies this window and its parent hierarchy.
  void NotifyWindowVisibilityChangedUp(Window* target);

  // Returns true if embed is allowed for this node. If embedding is allowed all
  // the children are removed.
  bool PrepareForEmbed();

  void RemoveTransientWindowImpl(Window* child);
  static void ReorderWithoutNotification(Window* window,
                                         Window* relative,
                                         mojom::OrderDirection direction);
  static bool ReorderImpl(Window* window,
                          Window* relative,
                          mojom::OrderDirection direction,
                          OrderChangedNotifier* notifier);

  // Returns a pointer to the stacking target that can be used by
  // RestackTransientDescendants.
  static Window** GetStackingTarget(Window* window);

  WindowTreeConnection* connection_;
  Id id_;
  Window* parent_;
  Children children_;

  Window* stacking_target_;
  Window* transient_parent_;
  Children transient_children_;

  base::ObserverList<WindowObserver> observers_;
  InputEventHandler* input_event_handler_;

  gfx::Rect bounds_;
  gfx::Insets client_area_;
  std::vector<gfx::Rect> additional_client_areas_;

  mojom::ViewportMetricsPtr viewport_metrics_;

  bool visible_;

  mojom::Cursor cursor_id_;

  SharedProperties properties_;

  // Drawn state is derived from the visible state and the parent's visible
  // state. This field is only used if the window has no parent (eg it's a
  // root).
  bool drawn_;

  // Value struct to keep the name and deallocator for this property.
  // Key cannot be used for this purpose because it can be char* or
  // WindowProperty<>.
  struct Value {
    const char* name;
    int64_t value;
    PropertyDeallocator deallocator;
  };

  std::map<const void*, Value> prop_map_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_H_
