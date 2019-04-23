# OverlayManager

OverlayManager is used to schedule the display of UI over the content area of a
WebState.

## Classes of note:

##### OverlayManager

OverlayManager is the public interface to the manager's functionality.  Clients
who wish to display overlay UI alongside a WebState's content area should use
OverlayManager::AddRequest() to schedule overlays to be displayed in the future.
Each OverlayManager manages overlays at a different level of modality, described
by OverlayModality.

##### OverlayManagerObserver

OverlayManagerObserver notifies interested classes of when overlay UI is
presented or dismissed.

##### OverlayRequest

OverlayRequests are passed to OverlayManager to trigger the display of overlay
UI over a WebState's content area.  Manager clients should create
OverlayRequests with an OverlayUserData subclass with the information necessary
to configure the overlay UI.  The config user data can later be extracted from
OverlayRequests by the manager's observers and UI delegate.

##### OverlayResponse

OverlayResponses are provided to each OverlayRequest to describe the user's
interaction with the overlay UI.  Manager clients should create OverlayResponses
with an OverlayUserData subclass with the overlay UI user interaction
information necessary to execute the callback for that overlay.

##### OverlayRequestQueue

Each WebState has an OverlayRequestQueue that stores the OverlayRequests for
that WebState.  The public interface exposes an immutable queue where the front
OverlayRequest is visible.  Internally, the queue is mutable and observable.

## Setting up OverlayManager:

Multiple OverlayManagers may be active for a single Browser to manage overlay UI
at different levels of modality (i.e. modal over WebState content area, modal
over entire browser, etc).  In order to use the manager, clients must retrieve
the OverlayManager associated with the key corresponding with the desired
modality.

Each instance of OverlayManager must be provided with an OverlayUIDelegate that
manages the overlay UI at the modality associated with the manager.

## Example usage of manager:

### Showing an alert with a title, message, an OK button, and a Cancel button

#####1. Create OverlayUserData subclasses for the requests and responses:

A request configuration user data should be created with the information
necessary to set up the overlay UI being requested.

    class AlertConfig : public OverlayUserData<AlertConfig> {
     public:
      const std::string& title() const;
      const std::string& message() const;
      const std::vector<std::string>& button\_titles() const;
     private:
      OVERLAY\_USER\_DATA\_SETUP(AlertConfig);
      AlertConfig(const std::string& title, const std::string& message);
    };

A response ino user data should be created with the information necessary to
execute the callback for the overlay.

    class AlertInfo : public OverlayUserData<AlertInfo> {
     public:
      const size\_t tapped\_button\_index() const;
     private:
      OVERLAY\_USER\_DATA\_SETUP(AlertInfo);
      AlertInfo(size\_t tapped\_button\_index);
    };

#####2. Request an overlay using the request config user data.

An OverlayRequest for the alert can be created using:

    OverlayRequest::CreateWithConfig<AlertConfig>(
        "alert title", "message text");

Manager clients can then supply this request to the OverlayManager for
scheduling over the web content area:

    OverlayModality modality =
        OverlayModality::kWebContentArea;
    OverlayManager::FromBrowser(browser, modality)->AddRequest(
        std::move(request), web_state);

#####3. Supply a response to the request.

An OverlayResponse for the alert can be created using:

    OverlayResponse::CreateWithInfo<AlertInfo>(0);


*NOTE: this manager is a work-in-progress, and this file only outlines how to
use the files currently in the repository.  It will be updated with more
complete instructions as more of the manager lands.*
