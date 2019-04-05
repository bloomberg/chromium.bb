# OverlayService

OverlayService is used to schedule the display of UI over the content area of a
WebState.

## Classes of note:

##### OverlayRequest

OverlayRequests are passed to OverlayService to trigger the display of overlay
UI over a WebState's content area.  Service clients should create
OverlayRequests with an OverlayUserData subclass with the information necessary
to configure the overlay UI.  The config user data can later be extracted from
OverlayRequests by the service's observers and UI delegate.

##### OverlayResponse

OverlayResponses are provided to each OverlayRequest to describe the user's
interaction with the overlay UI.  Service clients should create OverlayResponses
with an OverlayUserData subclass with the overlay UI user interaction
information necessary to execute the callback for that overlay.

##### OverlayRequestQueue

Each WebState has an OverlayRequestQueue that stores the OverlayRequests for
that WebState.  The public interface exposes an immutable queue where the front
OverlayRequest is visible.  Internally, the queue is mutable and observable.

## Example usage of service:

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

*TODO: insert OverlayService calls when interface is added.*

#####3. Supply a response to the request.

An OverlayResponse for the alert can be created using:

    OverlayResponse::CreateWithInfo<AlertInfo>(0);


*NOTE: this service is a work-in-progress, and this file only outlines how to
use the files currently in the repository.  It will be updated with more
complete instructions as more of the service lands.*
