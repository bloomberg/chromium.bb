// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTE_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTE_CONTROLLER_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/mojo/media_controller.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class BrowserContext;
}

namespace media_router {

class EventPageRequestManager;
class MediaRouter;

// A controller for a MediaRoute. Notifies its observers whenever there is a
// change in the route's MediaStatus. Forwards commands for controlling the
// route to a controller in the Media Router component extension if the
// extension is ready, and queues commands with EventPageRequestManager
// otherwise.
//
// It is owned by its observers, each of which holds a scoped_refptr to it. All
// the classes that hold a scoped_refptr must inherit from the Observer class.
// An observer should be instantiated with a scoped_refptr obtained through
// MediaRouter::GetRouteController().
//
// A MediaRouteController instance is destroyed when all its observers dispose
// their references to it. When the associated route is destroyed, Invalidate()
// is called to make the controller's observers dispose their refptrs.
class MediaRouteController : public mojom::MediaStatusObserver,
                             public base::RefCounted<MediaRouteController> {
 public:
  // Observes MediaRouteController for MediaStatus updates. The ownership of a
  // MediaRouteController is shared by its observers.
  class Observer {
   public:
    // Adds itself as an observer to |controller|.
    explicit Observer(scoped_refptr<MediaRouteController> controller);

    // Removes itself as an observer if |controller_| is still valid.
    virtual ~Observer();

    virtual void OnMediaStatusUpdated(const MediaStatus& status) = 0;

    // Returns a reference to the observed MediaRouteController. The reference
    // should not be stored by any object that does not subclass ::Observer.
    scoped_refptr<MediaRouteController> controller() const {
      return controller_;
    }

   protected:
    scoped_refptr<MediaRouteController> controller_;

   private:
    friend class MediaRouteController;

    // Disposes the reference to the controller.
    void InvalidateController();

    // Called by InvalidateController() after the reference to the controller is
    // disposed. Overridden by subclasses to do custom cleanup.
    virtual void OnControllerInvalidated();

    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Constructs a MediaRouteController that forwards media commands to
  // |mojo_media_controller|. |media_router| will be notified when the
  // MediaRouteController is destroyed via DetachRouteController().
  MediaRouteController(const MediaRoute::Id& route_id,
                       content::BrowserContext* context);

  // Media controller methods for forwarding commands to a
  // mojom::MediaControllerPtr held in |mojo_media_controller_|.
  virtual void Play();
  virtual void Pause();
  virtual void Seek(base::TimeDelta time);
  virtual void SetMute(bool mute);
  virtual void SetVolume(float volume);

  // mojom::MediaStatusObserver:
  // Notifies |observers_| of a status update.
  void OnMediaStatusUpdated(const MediaStatus& status) override;

  // Notifies |observers_| to dispose their references to the controller. The
  // controller gets destroyed when all the references are disposed.
  void Invalidate();

  // Returns an interface request tied to |mojo_media_controller_|, to be bound
  // to an implementation. |mojo_media_controller_| gets reset whenever this is
  // called.
  mojom::MediaControllerRequest CreateControllerRequest();

  // Returns a mojo pointer bound to |this| by |binding_|. This must only be
  // called at most once in the lifetime of the controller.
  mojom::MediaStatusObserverPtr BindObserverPtr();

  MediaRoute::Id route_id() const { return route_id_; }

  // Returns the latest media status that the controller has been notified with.
  // Returns a nullopt if the controller hasn't been notified yet.
  const base::Optional<MediaStatus>& current_media_status() const {
    return current_media_status_;
  }

 protected:
  ~MediaRouteController() override;

 private:
  friend class base::RefCounted<MediaRouteController>;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Called when the connection between |this| and the MediaControllerPtr or
  // the MediaStatusObserver binding is no longer valid. Notifies
  // |media_router_| and |observers_| to dispose their references to |this|.
  void OnMojoConnectionError();

  // The ID of the Media Route that |this| controls.
  const MediaRoute::Id route_id_;

  // Handle to the mojom::MediaController that receives media commands.
  mojom::MediaControllerPtr mojo_media_controller_;

  // |media_router_| will be notified when the controller is destroyed.
  MediaRouter* const media_router_;

  // Request manager responsible for waking the component extension and calling
  // the requests to it.
  EventPageRequestManager* const request_manager_;

  // The binding to observe the out-of-process provider of status updates.
  mojo::Binding<mojom::MediaStatusObserver> binding_;

  // Observers that are notified of status updates. The observers share the
  // ownership of the controller through scoped_refptr.
  base::ObserverList<Observer> observers_;

  // This becomes false when the controller is invalidated.
  bool is_valid_ = true;

  // The latest media status that the controller has been notified with.
  base::Optional<MediaStatus> current_media_status_;

  base::WeakPtrFactory<MediaRouteController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouteController);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTE_CONTROLLER_H_
