# OverlayManager

OverlayManager is used to schedule the display of UI alongside the content area
of a WebState.

## Classes of note:

##### OverlayRequest

OverlayRequests are model objects that are used to schedule the display of
UI alongside a WebState's content area.  They are created with an
OverlayUserData subclass containing the information necessary to configure the
requested UI.

##### OverlayResponse

OverlayResponses are provided to each OverlayRequest to describe the user's
interaction with the overlay UI.  Cients should create OverlayResponses with an
OverlayUserData subclass with the overlay UI user interaction information
necessary to execute the callback for that overlay.

##### OverlayRequestQueue

Each WebState has an OverlayRequestQueue at each OverlayModality that stores the
OverlayRequests for overlays to be displayed alongside that WebState's content
area.  When a client wishes to schedule the display of an overlay, it should
add an OverlayRequest to the desired WebState's queue.  This will trigger the
scheduling logic for that request's corresponding overlay UI.

##### OverlayManager

OverlayManager aggregates the scheduling logic for the OverlayRequests in the
queues for the WebStates in a particular Browser.  Clients must provide a UI
delegate to a Browser's manager that handles the presentation of overlay UI for
that manager's modality and Browser.

##### OverlayManagerObserver

OverlayManagerObserver notifies interested classes of when overlay UI is
presented or dismissed.

## Setting up OverlayManager:

Multiple OverlayManagers may be active for a single Browser to manage overlay UI
at different levels of modality (i.e. modal over WebState content area, modal
over entire browser, etc).  In order to use the manager, clients must retrieve
the OverlayManager associated with that OverlayModality.

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

A callback can be added to the request to use the response info:

    OverlayCallback callback =
        base::BindOnce(base::RetainBlock(^(OverlayResponse* response) {
      if (!response)
        return;
      AlertInfo* info = response->GetInfo<AlertInfo>();
      /* Handle button tap at info->tapped\_button\_index() */
    }));
    request->set_callback(std::move(callback));

Clients can then supply this request to the OverlayRequestQueue corresponding
with the WebState alongside which the overlay should be shown:

    OverlayModality modality =
        OverlayModality::kWebContentArea;
    OverlayRequestQueue::FromWebState(web_state, modality)->
        AddRequest(std::move(request));

#####3. Supply a response to the request.

Upon the user tapping a button on the alert, say at index 0, a response can be
created and supplied to that request.

    OverlayRequestQueue::FromWebState(web_state, modality)->front_request()->
        set_response(OverlayResponse::CreateWithInfo<AlertInfo>(0));

*NOTE: this manager is a work-in-progress, and this file only outlines how to
use the files currently in the repository.  It will be updated with more
complete instructions as more of the manager lands.*
