// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ibus_ui_controller.h"

#if defined(HAVE_IBUS)
#include <ibus.h>
#endif

#include <sstream>
#include "base/logging.h"
#include "base/string_util.h"
#include "third_party/mozc/session/candidates_lite.pb.h"

namespace chromeos {
namespace input_method {

InputMethodLookupTable::InputMethodLookupTable()
    : visible(false),
      cursor_absolute_index(0),
      page_size(0),
      orientation(kHorizontal) {
}

InputMethodLookupTable::~InputMethodLookupTable() {
}

std::string InputMethodLookupTable::ToString() const {
  std::stringstream stream;
  stream << "visible: " << visible << "\n";
  stream << "cursor_absolute_index: " << cursor_absolute_index << "\n";
  stream << "page_size: " << page_size << "\n";
  stream << "orientation: " << orientation << "\n";
  stream << "candidates:";
  for (size_t i = 0; i < candidates.size(); ++i) {
    stream << " [" << candidates[i] << "]";
  }
  stream << "\nlabels:";
  for (size_t i = 0; i < labels.size(); ++i) {
    stream << " [" << labels[i] << "]";
  }
  return stream.str();
}

#if defined(HAVE_IBUS)

// Returns an string representation of |table| for debugging.
std::string IBusLookupTableToString(IBusLookupTable* table) {
  std::stringstream stream;
  stream << "page_size: " << table->page_size << "\n";
  stream << "cursor_pos: " << table->cursor_pos << "\n";
  stream << "cursor_visible: " << table->cursor_visible << "\n";
  stream << "round: " << table->round << "\n";
  stream << "orientation: " << table->orientation << "\n";
  stream << "candidates:";
  for (int i = 0; ; i++) {
    IBusText *text = ibus_lookup_table_get_candidate(table, i);
    if (!text) {
      break;
    }
    stream << " " << text->text;
  }
  return stream.str();
}

// The real implementation of the IBusUiController.
class IBusUiControllerImpl : public IBusUiController {
 public:
  IBusUiControllerImpl()
      : ibus_(NULL),
        ibus_panel_service_(NULL) {
  }

  ~IBusUiControllerImpl() {
    // ibus_panel_service_ depends on ibus_, thus unref it first.
    if (ibus_panel_service_) {
      DisconnectPanelServiceSignals();
      g_object_unref(ibus_panel_service_);
    }
    if (ibus_) {
      DisconnectIBusSignals();
      g_object_unref(ibus_);
    }
  }

  // Creates IBusBus object if it's not created yet, and tries to connect to
  // ibus-daemon. Returns true if IBusBus is successfully connected to the
  // daemon.
  bool ConnectToIBus() {
    if (ibus_) {
      return true;
    }
    ibus_init();
    ibus_ = ibus_bus_new();
    CHECK(ibus_) << "ibus_bus_new() failed. Out of memory?";

    bool result = false;
    // Check the IBus connection status.
    if (ibus_bus_is_connected(ibus_)) {
      LOG(INFO) << "ibus_bus_is_connected(). IBus connection is ready.";
      FOR_EACH_OBSERVER(Observer, observers_, OnConnectionChange(true));
      result = true;
    }

    // Start listening the gobject signals regardless of the bus connection
    // status.
    ConnectIBusSignals();
    return result;
  }

  // Creates IBusPanelService object if |ibus_| is already connected.
  bool MaybeRestorePanelService() {
    if (!ibus_ || !ibus_bus_is_connected(ibus_)) {
      return false;
    }

    if (ibus_panel_service_) {
      LOG(ERROR) << "IBusPanelService is already available. Remove it first.";
      g_object_set_data(G_OBJECT(ibus_), kPanelObjectKey, NULL);
      g_object_unref(ibus_panel_service_);
      ibus_panel_service_ = NULL;
    }

    // Create an IBusPanelService object.
    GDBusConnection* ibus_connection = ibus_bus_get_connection(ibus_);
    if (!ibus_connection) {
      LOG(ERROR) << "ibus_bus_get_connection() failed";
      return false;
    }
    ibus_panel_service_ = ibus_panel_service_new(ibus_connection);
    if (!ibus_panel_service_) {
      LOG(ERROR) << "ibus_chromeos_panel_service_new() failed";
      return false;
    }
    ConnectPanelServiceSignals();
    g_object_set_data(G_OBJECT(ibus_), kPanelObjectKey, ibus_panel_service_);
    LOG(INFO) << "IBusPanelService object is successfully (re-)created.";

    // Request the well-known name *asynchronously*.
    ibus_bus_request_name_async(ibus_,
                                IBUS_SERVICE_PANEL,
                                0  /* flags */,
                                -1  /* timeout */,
                                NULL  /* cancellable */,
                                RequestNameCallback,
                                g_object_ref(ibus_));
    return true;
  }

  // IBusUiController override.
  virtual void NotifyCandidateClicked(int index, int button, int flags) {
    if (!ibus_ || !ibus_bus_is_connected(ibus_)) {
      LOG(ERROR) << "NotifyCandidateClicked: bus is not connected.";
      return;
    }
    if (!ibus_panel_service_) {
      LOG(ERROR) << "NotifyCandidateClicked: panel service is not available.";
      return;
    }

    /* Send a D-Bus signal to ibus-daemon *asynchronously*. */
    ibus_panel_service_candidate_clicked(ibus_panel_service_,
                                         index,
                                         button,
                                         flags);
  }

  // IBusUiController override.
  virtual void NotifyCursorUp() {
    if (!ibus_ || !ibus_bus_is_connected(ibus_)) {
      LOG(ERROR) << "NotifyCursorUp: bus is not connected.";
      return;
    }
    if (!ibus_panel_service_) {
      LOG(ERROR) << "NotifyCursorUp: panel service is not available.";
      return;
    }

    /* Send a D-Bus signal to ibus-daemon *asynchronously*. */
    ibus_panel_service_cursor_up(ibus_panel_service_);
  }

  // IBusUiController override.
  virtual void NotifyCursorDown() {
    if (!ibus_ || !ibus_bus_is_connected(ibus_)) {
      LOG(ERROR) << "NotifyCursorDown: bus is not connected.";
      return;
    }
    if (!ibus_panel_service_) {
      LOG(ERROR) << "NotifyCursorDown: panel service is not available.";
      return;
    }
     /* Send a D-Bus signal to ibus-daemon *asynchronously*. */
    ibus_panel_service_cursor_down(ibus_panel_service_);
  }

  // IBusUiController override.
  virtual void NotifyPageUp() {
    if (!ibus_ || !ibus_bus_is_connected(ibus_)) {
      LOG(ERROR) << "NotifyPageUp: bus is not connected.";
      return;
    }
    if (!ibus_panel_service_) {
      LOG(ERROR) << "NotifyPageUp: panel service is not available.";
      return;
    }

    /* Send a D-Bus signal to ibus-daemon *asynchronously*. */
    ibus_panel_service_page_up(ibus_panel_service_);
  }

  // IBusUiController override.
  virtual void NotifyPageDown() {
    if (!ibus_ || !ibus_bus_is_connected(ibus_)) {
      LOG(ERROR) << "NotifyPageDown: bus is not connected.";
      return;
    }
    if (!ibus_panel_service_) {
      LOG(ERROR) << "NotifyPageDown: panel service is not available.";
      return;
    }

    /* Send a D-Bus signal to ibus-daemon *asynchronously*. */
    ibus_panel_service_page_down(ibus_panel_service_);
  }

  // IBusUiController override.
  virtual void Connect() {
    // It's totally fine if ConnectToIBus() fails here, as we'll get
    // "connected" gobject signal once the connection becomes ready.
    if (ConnectToIBus())
      MaybeRestorePanelService();
  }

  // IBusUiController override.
  virtual void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  // IBusUiController override.
  virtual void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

 private:
  // Functions that end with Thunk are used to deal with glib callbacks.
  //
  // Note that we cannot use CHROMEG_CALLBACK_0() here as we'll define
  // IBusBusConnected() inline. If we are to define the function outside
  // of the class definition, we should use CHROMEG_CALLBACK_0() here.
  //
  // CHROMEG_CALLBACK_0(Impl,
  //                    void, IBusBusConnected, IBusBus*);
  static void IBusBusConnectedThunk(IBusBus* sender, gpointer userdata) {
    return reinterpret_cast<IBusUiControllerImpl*>(userdata)
        ->IBusBusConnected(sender);
  }
  static void IBusBusDisconnectedThunk(IBusBus* sender, gpointer userdata) {
    return reinterpret_cast<IBusUiControllerImpl*>(userdata)
        ->IBusBusDisconnected(sender);
  }
  static void HideAuxiliaryTextThunk(IBusPanelService* sender,
                                     gpointer userdata) {
    return reinterpret_cast<IBusUiControllerImpl*>(userdata)
        ->HideAuxiliaryText(sender);
  }
  static void HideLookupTableThunk(IBusPanelService* sender,
                                   gpointer userdata) {
    return reinterpret_cast<IBusUiControllerImpl*>(userdata)
        ->HideLookupTable(sender);
  }
  static void UpdateAuxiliaryTextThunk(IBusPanelService* sender,
                                       IBusText* text, gboolean visible,
                                       gpointer userdata) {
    return reinterpret_cast<IBusUiControllerImpl*>(userdata)
        ->UpdateAuxiliaryText(sender, text, visible);
  }
  static void SetCursorLocationThunk(IBusPanelService* sender,
                                     gint x, gint y, gint width, gint height,
                                     gpointer userdata) {
    return reinterpret_cast<IBusUiControllerImpl*>(userdata)
        ->SetCursorLocation(sender, x, y, width, height);
  }
  static void UpdateLookupTableThunk(IBusPanelService* sender,
                                     IBusLookupTable* table, gboolean visible,
                                     gpointer userdata) {
    return reinterpret_cast<IBusUiControllerImpl*>(userdata)
        ->UpdateLookupTable(sender, table, visible);
  }
  static void UpdatePreeditTextThunk(IBusPanelService* sender,
                                     IBusText* text,
                                     guint cursor_pos,
                                     gboolean visible,
                                     gpointer userdata) {
    return reinterpret_cast<IBusUiControllerImpl*>(userdata)
        ->UpdatePreeditText(sender, text, cursor_pos, visible);
  }
  static void HidePreeditTextThunk(IBusPanelService* sender,
                                   gpointer userdata) {
    return reinterpret_cast<IBusUiControllerImpl*>(userdata)
        ->HidePreeditText(sender);
  }


  // Installs gobject signal handlers to |ibus_|.
  void ConnectIBusSignals() {
    if (!ibus_) {
      return;
    }
    g_signal_connect(ibus_,
                     "connected",
                     G_CALLBACK(IBusBusConnectedThunk),
                     this);
    g_signal_connect(ibus_,
                     "disconnected",
                     G_CALLBACK(IBusBusDisconnectedThunk),
                     this);
  }

  // Removes gobject signal handlers from |ibus_|.
  void DisconnectIBusSignals() {
    if (!ibus_) {
      return;
    }
    g_signal_handlers_disconnect_by_func(
        ibus_,
        reinterpret_cast<gpointer>(G_CALLBACK(IBusBusConnectedThunk)),
        this);
    g_signal_handlers_disconnect_by_func(
        ibus_,
        reinterpret_cast<gpointer>(G_CALLBACK(IBusBusDisconnectedThunk)),
        this);
  }

  // Installs gobject signal handlers to |ibus_panel_service_|.
  void ConnectPanelServiceSignals() {
    if (!ibus_panel_service_) {
      return;
    }
    g_signal_connect(ibus_panel_service_,
                     "hide-auxiliary-text",
                     G_CALLBACK(HideAuxiliaryTextThunk),
                     this);
    g_signal_connect(ibus_panel_service_,
                     "hide-lookup-table",
                     G_CALLBACK(HideLookupTableThunk),
                     this);
    g_signal_connect(ibus_panel_service_,
                     "update-auxiliary-text",
                     G_CALLBACK(UpdateAuxiliaryTextThunk),
                     this);
    g_signal_connect(ibus_panel_service_,
                     "set-cursor-location",
                     G_CALLBACK(SetCursorLocationThunk),
                     this);
    g_signal_connect(ibus_panel_service_,
                     "update-lookup-table",
                     G_CALLBACK(UpdateLookupTableThunk),
                     this);
    g_signal_connect(ibus_panel_service_,
                     "update-preedit-text",
                     G_CALLBACK(UpdatePreeditTextThunk),
                     this);
    g_signal_connect(ibus_panel_service_,
                     "hide-preedit-text",
                     G_CALLBACK(HidePreeditTextThunk),
                     this);
  }

  // Removes gobject signal handlers from |ibus_panel_service_|.
  void DisconnectPanelServiceSignals() {
    if (!ibus_panel_service_) {
      return;
    }
    g_signal_handlers_disconnect_by_func(
        ibus_panel_service_,
        reinterpret_cast<gpointer>(HideAuxiliaryTextThunk),
        this);
    g_signal_handlers_disconnect_by_func(
        ibus_panel_service_,
        reinterpret_cast<gpointer>(HideLookupTableThunk),
        this);
    g_signal_handlers_disconnect_by_func(
        ibus_panel_service_,
        reinterpret_cast<gpointer>(UpdateAuxiliaryTextThunk),
        this);
    g_signal_handlers_disconnect_by_func(
        ibus_panel_service_,
        reinterpret_cast<gpointer>(SetCursorLocationThunk),
        this);
    g_signal_handlers_disconnect_by_func(
        ibus_panel_service_,
        reinterpret_cast<gpointer>(UpdateLookupTableThunk),
        this);
    g_signal_handlers_disconnect_by_func(
        ibus_panel_service_,
        reinterpret_cast<gpointer>(UpdatePreeditTextThunk),
        this);
    g_signal_handlers_disconnect_by_func(
        ibus_panel_service_,
        reinterpret_cast<gpointer>(HidePreeditTextThunk),
        this);
  }

  // Handles "connected" signal from ibus-daemon.
  void IBusBusConnected(IBusBus* bus) {
    LOG(WARNING) << "IBus connection is recovered.";
    if (!MaybeRestorePanelService()) {
      LOG(ERROR) << "MaybeRestorePanelService() failed";
      return;
    }

    FOR_EACH_OBSERVER(Observer, observers_, OnConnectionChange(true));
  }

  // Handles "disconnected" signal from ibus-daemon. Releases the
  // |ibus_panel_service_| object since the connection the service has will be
  // destroyed soon.
  void IBusBusDisconnected(IBusBus* bus) {
    LOG(WARNING) << "IBus connection is terminated.";
    if (ibus_panel_service_) {
      DisconnectPanelServiceSignals();
      // Since the connection being disconnected is currently mutex-locked,
      // we can't unref the panel service object directly here. Because when the
      // service object is deleted, the connection, which the service also has,
      // will be locked again. To avoid deadlock, we use g_idle_add instead.
      g_object_set_data(G_OBJECT(ibus_), kPanelObjectKey, NULL);
      g_idle_add(ReleasePanelService, ibus_panel_service_);
      ibus_panel_service_ = NULL;
    }

    FOR_EACH_OBSERVER(Observer, observers_, OnConnectionChange(false));
  }

  // Releases |ibus_panel_service_|. See the comment above.
  static gboolean ReleasePanelService(gpointer user_data) {
    g_return_val_if_fail(IBUS_IS_PANEL_SERVICE(user_data), FALSE);
    g_object_unref(user_data);
    return FALSE;  // stop the idle timer.
  }

  // Handles IBusPanelService's |HideAuxiliaryText| method call.
  void HideAuxiliaryText(IBusPanelService* panel) {
    FOR_EACH_OBSERVER(Observer, observers_, OnHideAuxiliaryText());
  }

  // Handles IBusPanelService's |HideLookupTable| method call.
  void HideLookupTable(IBusPanelService *panel) {
    FOR_EACH_OBSERVER(Observer, observers_, OnHideLookupTable());
  }

  // Handles IBusPanelService's |UpdateAuxiliaryText| method call.
  void UpdateAuxiliaryText(IBusPanelService* panel,
                           IBusText* text,
                           gboolean visible) {
    g_return_if_fail(text);
    g_return_if_fail(text->text);
    // Convert IBusText to a std::string. IBusText is an attributed text,
    const std::string simple_text = text->text;
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnUpdateAuxiliaryText(simple_text, visible == TRUE));
  }

  // Handles IBusPanelService's |SetCursorLocation| method call.
  void SetCursorLocation(IBusPanelService *panel,
                         gint x,
                         gint y,
                         gint width,
                         gint height) {
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnSetCursorLocation(x, y, width, height));
  }

  // Handles IBusPanelService's |UpdatePreeditText| method call.
  void UpdatePreeditText(IBusPanelService *panel,
                         IBusText *text,
                         guint cursor_pos,
                         gboolean visible) {
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnUpdatePreeditText(text->text, cursor_pos, visible));
  }

  // Handles IBusPanelService's |UpdatePreeditText| method call.
  void HidePreeditText(IBusPanelService *panel) {
    FOR_EACH_OBSERVER(Observer, observers_, OnHidePreeditText());
  }

  // Handles IBusPanelService's |UpdateLookupTable| method call.
  void UpdateLookupTable(IBusPanelService *panel,
                         IBusLookupTable *table,
                         gboolean visible) {
    g_return_if_fail(table);

    InputMethodLookupTable lookup_table;
    lookup_table.visible = (visible == TRUE);

    // Copy the orientation information.
    const gint orientation = ibus_lookup_table_get_orientation(table);
    if (orientation == IBUS_ORIENTATION_VERTICAL) {
      lookup_table.orientation = InputMethodLookupTable::kVertical;
    } else if (orientation == IBUS_ORIENTATION_HORIZONTAL) {
      lookup_table.orientation = InputMethodLookupTable::kHorizontal;
    }

#ifdef OS_CHROMEOS
    // The function ibus_serializable_get_attachment had been changed
    // to use GVariant by the commit
    // https://github.com/ibus/ibus/commit/ac9dfac13cef34288440a2ecdf067cd827fb2f8f
    GVariant* variant = ibus_serializable_get_attachment(
        IBUS_SERIALIZABLE(table), "mozc.candidates");
    if (variant != NULL) {
      gconstpointer ptr = g_variant_get_data(variant);
      if (ptr != NULL) {
        gsize size = g_variant_get_size(variant);
        GByteArray* bytearray = g_byte_array_sized_new(size);
        g_byte_array_append(
            bytearray, reinterpret_cast<const guint8*>(ptr), size);
        if (!lookup_table.mozc_candidates.ParseFromArray(
               bytearray->data, bytearray->len)) {
          lookup_table.mozc_candidates.Clear();
        }
        g_byte_array_unref(bytearray);
      }
    }
#endif
    // Copy candidates and annotations to |lookup_table|.
    for (int i = 0; ; i++) {
      IBusText *text = ibus_lookup_table_get_candidate(table, i);
      if (!text) {
        break;
      }
      lookup_table.candidates.push_back(text->text);

      const mozc::commands::Candidates &candidates =
          lookup_table.mozc_candidates;
      if ((i < candidates.candidate_size()) &&
          candidates.candidate(i).has_annotation() &&
          candidates.candidate(i).annotation().has_description()) {
        lookup_table.annotations.push_back(
            candidates.candidate(i).annotation().description());
      } else {
        lookup_table.annotations.push_back("");
      }
    }
    DCHECK_EQ(lookup_table.candidates.size(),
              lookup_table.annotations.size());

    // Copy labels to |lookup_table|.
    for (int i = 0; ; i++) {
      IBusText *text = ibus_lookup_table_get_label(table, i);
      if (!text) {
        break;
      }
      lookup_table.labels.push_back(text->text);
    }

    lookup_table.cursor_absolute_index =
        ibus_lookup_table_get_cursor_pos(table);
    lookup_table.page_size = ibus_lookup_table_get_page_size(table);
    // Ensure that the page_size is non-zero to avoid div-by-zero error.
    if (lookup_table.page_size <= 0) {
      LOG(DFATAL) << "Invalid page size: " << lookup_table.page_size;
      lookup_table.page_size = 1;
    }

    FOR_EACH_OBSERVER(Observer, observers_,
                      OnUpdateLookupTable(lookup_table));
  }

  // A callback function that will be called when ibus_bus_request_name_async()
  // request is finished.
  static void RequestNameCallback(GObject* source_object,
                                  GAsyncResult* res,
                                  gpointer user_data) {
    IBusBus* bus = IBUS_BUS(user_data);
    g_return_if_fail(bus);

    GError* error = NULL;
    const guint service_id =
        ibus_bus_request_name_async_finish(bus, res, &error);

    if (!service_id) {
      std::string message = "(unknown error)";
      if (error && error->message) {
        message = error->message;
      }
      LOG(ERROR) << "Failed to register the panel service: " << message;
    } else {
      LOG(INFO) << "The panel service is registered: ID=" << service_id;
    }

    if (error) {
      g_error_free(error);
    }
    g_object_unref(bus);
  }

  IBusBus* ibus_;
  IBusPanelService* ibus_panel_service_;
  ObserverList<Observer> observers_;
};
#endif  // defined(HAVE_IBUS)

// The stub implementation is used if IBus is not present.
//
// Note that this class is intentionally built even if HAVE_IBUS is
// defined so that we can easily tell build breakage when we change the
// IBusUiControllerImpl but forget to update the stub implementation.
class IBusUiControllerStubImpl : public IBusUiController {
 public:
  IBusUiControllerStubImpl() {
  }

  virtual void Connect() {
  }

  virtual void AddObserver(Observer* observer) {
  }

  virtual void RemoveObserver(Observer* observer) {
  }

  virtual void NotifyCandidateClicked(int index, int button, int flags) {
  }

  virtual void NotifyCursorUp() {
  }

  virtual void NotifyCursorDown() {
  }

  virtual void NotifyPageUp() {
  }

  virtual void NotifyPageDown() {
  }
};

IBusUiController* IBusUiController::Create() {
#if defined(HAVE_IBUS)
  return new IBusUiControllerImpl;
#else
  return new IBusUiControllerStubImpl;
#endif
}

IBusUiController::~IBusUiController() {
}

}  // namespace input_method
}  // namespace chromeos
