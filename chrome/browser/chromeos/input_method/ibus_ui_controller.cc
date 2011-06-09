// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_input_method_ui.h"

#include <base/logging.h>
#include <base/string_util.h>
#include <base/utf_string_conversions.h>
#include <ibus.h>

namespace chromeos {

namespace {

// Checks the attribute if this indicates annotation.
gboolean IsAnnotation(IBusAttribute *attr) {
  g_return_val_if_fail(attr, FALSE);

  // Define annotation text color.
  static const guint kAnnotationColor = 0x888888;

  // Currently, we can discriminate annotation by specific value |attr->value|
  // TODO(nhiroki): We should change the way when iBus supports annotations.
  if (attr->type == IBUS_ATTR_TYPE_FOREGROUND &&
      attr->value == kAnnotationColor) {
    return TRUE;
  }
  return FALSE;
}

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

}  // namespace

// A thin wrapper for IBusPanelService.
class InputMethodUiStatusConnection {
 public:
  InputMethodUiStatusConnection(
      const InputMethodUiStatusMonitorFunctions& monitor_functions,
      void* input_method_library)
      : monitor_functions_(monitor_functions),
        connection_change_handler_(NULL),
        input_method_library_(input_method_library),
        ibus_(NULL),
        ibus_panel_service_(NULL) {
  }

  ~InputMethodUiStatusConnection() {
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
      if (connection_change_handler_) {
        connection_change_handler_(input_method_library_, true);
      }
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

  // A function called when a user clicks the candidate_window.
  bool NotifyCandidateClicked(int index, int button, int flags) {
    if (!ibus_ || !ibus_bus_is_connected(ibus_)) {
      LOG(ERROR) << "NotifyCandidateClicked: bus is not connected.";
      return false;
    }
    if (!ibus_panel_service_) {
      LOG(ERROR) << "NotifyCandidateClicked: panel service is not available.";
      return false;
    }

    /* Send a D-Bus signal to ibus-daemon *asynchronously*. */
    ibus_panel_service_candidate_clicked(ibus_panel_service_,
                                         index,
                                         button,
                                         flags);
    return true;
  }

  // A function called when a user clicks the cursor up button.
  bool NotifyCursorUp() {
    if (!ibus_ || !ibus_bus_is_connected(ibus_)) {
      LOG(ERROR) << "NotifyCursorUp: bus is not connected.";
      return false;
    }
    if (!ibus_panel_service_) {
      LOG(ERROR) << "NotifyCursorUp: panel service is not available.";
      return false;
    }

    /* Send a D-Bus signal to ibus-daemon *asynchronously*. */
    ibus_panel_service_cursor_up(ibus_panel_service_);
    return true;
  }

  // A function called when a user clicks the cursor down button.
  bool NotifyCursorDown() {
    if (!ibus_ || !ibus_bus_is_connected(ibus_)) {
      LOG(ERROR) << "NotifyCursorDown: bus is not connected.";
      return false;
    }
    if (!ibus_panel_service_) {
      LOG(ERROR) << "NotifyCursorDown: panel service is not available.";
      return false;
    }
     /* Send a D-Bus signal to ibus-daemon *asynchronously*. */
    ibus_panel_service_cursor_down(ibus_panel_service_);
    return true;
  }

  // A function called when a user clicks the page up button.
  bool NotifyPageUp() {
    if (!ibus_ || !ibus_bus_is_connected(ibus_)) {
      LOG(ERROR) << "NotifyPageUp: bus is not connected.";
      return false;
    }
    if (!ibus_panel_service_) {
      LOG(ERROR) << "NotifyPageUp: panel service is not available.";
      return false;
    }

    /* Send a D-Bus signal to ibus-daemon *asynchronously*. */
    ibus_panel_service_page_up(ibus_panel_service_);
    return true;
  }

  // A function called when a user clicks the page down button.
  bool NotifyPageDown() {
    if (!ibus_ || !ibus_bus_is_connected(ibus_)) {
      LOG(ERROR) << "NotifyPageDown: bus is not connected.";
      return false;
    }
    if (!ibus_panel_service_) {
      LOG(ERROR) << "NotifyPageDown: panel service is not available.";
      return false;
    }

    /* Send a D-Bus signal to ibus-daemon *asynchronously*. */
    ibus_panel_service_page_down(ibus_panel_service_);
    return true;
  }


  // Registers a callback function which is called when IBusBus connection
  // status is changed.
  void MonitorInputMethodConnection(
      InputMethodConnectionChangeMonitorFunction connection_change_handler) {
    connection_change_handler_ = connection_change_handler;
  }

 private:
  // Installs gobject signal handlers to |ibus_|.
  void ConnectIBusSignals() {
    if (!ibus_) {
      return;
    }
    g_signal_connect(ibus_,
                     "connected",
                     G_CALLBACK(IBusBusConnectedCallback),
                     this);
    g_signal_connect(ibus_,
                     "disconnected",
                     G_CALLBACK(IBusBusDisconnectedCallback),
                     this);
  }

  // Removes gobject signal handlers from |ibus_|.
  void DisconnectIBusSignals() {
    if (!ibus_) {
      return;
    }
    g_signal_handlers_disconnect_by_func(
        ibus_,
        reinterpret_cast<gpointer>(G_CALLBACK(IBusBusConnectedCallback)),
        this);
    g_signal_handlers_disconnect_by_func(
        ibus_,
        reinterpret_cast<gpointer>(G_CALLBACK(IBusBusDisconnectedCallback)),
        this);
  }

  // Installs gobject signal handlers to |ibus_panel_service_|.
  void ConnectPanelServiceSignals() {
    if (!ibus_panel_service_) {
      return;
    }
    g_signal_connect(ibus_panel_service_,
                     "hide-auxiliary-text",
                     G_CALLBACK(HideAuxiliaryTextCallback),
                     this);
    g_signal_connect(ibus_panel_service_,
                     "hide-lookup-table",
                     G_CALLBACK(HideLookupTableCallback),
                     this);
    g_signal_connect(ibus_panel_service_,
                     "update-auxiliary-text",
                     G_CALLBACK(UpdateAuxiliaryTextCallback),
                     this);
    g_signal_connect(ibus_panel_service_,
                     "set-cursor-location",
                     G_CALLBACK(SetCursorLocationCallback),
                     this);
    g_signal_connect(ibus_panel_service_,
                     "update-lookup-table",
                     G_CALLBACK(UpdateLookupTableCallback),
                     this);
  }

  // Removes gobject signal handlers from |ibus_panel_service_|.
  void DisconnectPanelServiceSignals() {
    if (!ibus_panel_service_) {
      return;
    }
    g_signal_handlers_disconnect_by_func(
        ibus_panel_service_,
        reinterpret_cast<gpointer>(HideAuxiliaryTextCallback),
        this);
    g_signal_handlers_disconnect_by_func(
        ibus_panel_service_,
        reinterpret_cast<gpointer>(HideLookupTableCallback),
        this);
    g_signal_handlers_disconnect_by_func(
        ibus_panel_service_,
        reinterpret_cast<gpointer>(UpdateAuxiliaryTextCallback),
        this);
    g_signal_handlers_disconnect_by_func(
        ibus_panel_service_,
        reinterpret_cast<gpointer>(SetCursorLocationCallback),
        this);
    g_signal_handlers_disconnect_by_func(
        ibus_panel_service_,
        reinterpret_cast<gpointer>(UpdateLookupTableCallback),
        this);
  }

  // Handles "connected" signal from ibus-daemon.
  static void IBusBusConnectedCallback(IBusBus* bus, gpointer user_data) {
    LOG(WARNING) << "IBus connection is recovered.";
    g_return_if_fail(user_data);
    InputMethodUiStatusConnection* self
        = static_cast<InputMethodUiStatusConnection*>(user_data);
    if (!self->MaybeRestorePanelService()) {
      LOG(ERROR) << "MaybeRestorePanelService() failed";
      return;
    }
    if (self->connection_change_handler_) {
      self->connection_change_handler_(self->input_method_library_, true);
    }
  }

  // Handles "disconnected" signal from ibus-daemon. Releases the
  // |ibus_panel_service_| object since the connection the service has will be
  // destroyed soon.
  static void IBusBusDisconnectedCallback(IBusBus* bus, gpointer user_data) {
    LOG(WARNING) << "IBus connection is terminated.";
    g_return_if_fail(user_data);
    InputMethodUiStatusConnection* self
        = static_cast<InputMethodUiStatusConnection*>(user_data);
    if (self->ibus_panel_service_) {
      self->DisconnectPanelServiceSignals();
      // Since the connection being disconnected is currently mutex-locked,
      // we can't unref the panel service object directly here. Because when the
      // service object is deleted, the connection, which the service also has,
      // will be locked again. To avoid deadlock, we use g_idle_add instead.
      g_object_set_data(G_OBJECT(self->ibus_), kPanelObjectKey, NULL);
      g_idle_add(ReleasePanelService, self->ibus_panel_service_);
      self->ibus_panel_service_ = NULL;
    }

    if (self->connection_change_handler_) {
      self->connection_change_handler_(self->input_method_library_, false);
    }
  }

  // Releases |ibus_panel_service_|. See the comment above.
  static gboolean ReleasePanelService(gpointer user_data) {
    g_return_val_if_fail(IBUS_IS_PANEL_SERVICE(user_data), FALSE);
    g_object_unref(user_data);
    return FALSE;  // stop the idle timer.
  }

  // Handles IBusPanelService's |HideAuxiliaryText| method call.
  // Calls |hide_auxiliary_text| in |monitor_functions|.
  static void HideAuxiliaryTextCallback(IBusPanelService *panel,
                                        gpointer user_data) {
    g_return_if_fail(user_data);
    InputMethodUiStatusConnection* self
        = static_cast<InputMethodUiStatusConnection*>(user_data);
    g_return_if_fail(self->monitor_functions_.hide_auxiliary_text);
    self->monitor_functions_.hide_auxiliary_text(self->input_method_library_);
  }

  // Handles IBusPanelService's |HideLookupTable| method call.
  // Calls |hide_lookup_table| in |monitor_functions|.
  static void HideLookupTableCallback(IBusPanelService *panel,
                                      gpointer user_data) {
    g_return_if_fail(user_data);
    InputMethodUiStatusConnection* self
        = static_cast<InputMethodUiStatusConnection*>(user_data);
    g_return_if_fail(self->monitor_functions_.hide_lookup_table);
    self->monitor_functions_.hide_lookup_table(self->input_method_library_);
  }

  // Handles IBusPanelService's |UpdateAuxiliaryText| method call.
  // Converts IBusText to a std::string, and calls |update_auxiliary_text| in
  // |monitor_functions|
  static void UpdateAuxiliaryTextCallback(IBusPanelService *panel,
                                          IBusText *text,
                                          gboolean visible,
                                          gpointer user_data) {
    g_return_if_fail(text);
    g_return_if_fail(text->text);
    g_return_if_fail(user_data);
    InputMethodUiStatusConnection* self
        = static_cast<InputMethodUiStatusConnection*>(user_data);
    g_return_if_fail(self->monitor_functions_.update_auxiliary_text);
    // Convert IBusText to a std::string. IBusText is an attributed text,
    const std::string simple_text = text->text;
    self->monitor_functions_.update_auxiliary_text(
        self->input_method_library_, simple_text, visible == TRUE);
  }

  // Handles IBusPanelService's |SetCursorLocation| method call.
  // Calls |set_cursor_location| in |monitor_functions|.
  static void SetCursorLocationCallback(IBusPanelService *panel,
                                        gint x,
                                        gint y,
                                        gint width,
                                        gint height,
                                        gpointer user_data) {
    g_return_if_fail(user_data);
    InputMethodUiStatusConnection* self
        = static_cast<InputMethodUiStatusConnection*>(user_data);
    g_return_if_fail(self->monitor_functions_.set_cursor_location);
    self->monitor_functions_.set_cursor_location(
        self->input_method_library_, x, y, width, height);
  }

  // Handles IBusPanelService's |UpdateLookupTable| method call.
  // Creates an InputMethodLookupTable object and calls |update_lookup_table| in
  // |monitor_functions|
  static void UpdateLookupTableCallback(IBusPanelService *panel,
                                        IBusLookupTable *table,
                                        gboolean visible,
                                        gpointer user_data) {
    g_return_if_fail(table);
    g_return_if_fail(user_data);
    InputMethodUiStatusConnection* self
        = static_cast<InputMethodUiStatusConnection*>(user_data);
    g_return_if_fail(self->monitor_functions_.update_lookup_table);

    InputMethodLookupTable lookup_table;
    lookup_table.visible = (visible == TRUE);

    // Copy the orientation information.
    const gint orientation = ibus_lookup_table_get_orientation(table);
    if (orientation == IBUS_ORIENTATION_VERTICAL) {
      lookup_table.orientation = InputMethodLookupTable::kVertical;
    } else if (orientation == IBUS_ORIENTATION_HORIZONTAL) {
      lookup_table.orientation = InputMethodLookupTable::kHorizontal;
    }

    // Copy candidates and annotations to |lookup_table|.
    for (int i = 0; ; i++) {
      IBusText *text = ibus_lookup_table_get_candidate(table, i);
      if (!text) {
        break;
      }

      if (!text->attrs || !text->attrs->attributes) {
        lookup_table.candidates.push_back(text->text);
        lookup_table.annotations.push_back("");
        continue;
      }

      // Divide candidate and annotation by specific attribute.
      const guint length = text->attrs->attributes->len;
      for (int j = 0; ; j++) {
        IBusAttribute *attr = ibus_attr_list_get(text->attrs, j);

        // The candidate does not have annotation.
        if (!attr) {
          lookup_table.candidates.push_back(text->text);
          lookup_table.annotations.push_back("");
          break;
        }

        // Check that the attribute indicates annotation.
        if (IsAnnotation(attr) && j + 1 == static_cast<int>(length)) {
        const std::wstring candidate_word =
            UTF8ToWide(text->text).substr(0, attr->start_index);
        lookup_table.candidates.push_back(WideToUTF8(candidate_word));

        const std::wstring annotation_word =
            UTF8ToWide(text->text).substr(attr->start_index, attr->end_index);
        lookup_table.annotations.push_back(WideToUTF8(annotation_word));

        break;
        }
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

    self->monitor_functions_.update_lookup_table(
        self->input_method_library_, lookup_table);
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

  InputMethodUiStatusMonitorFunctions monitor_functions_;
  InputMethodConnectionChangeMonitorFunction connection_change_handler_;
  void* input_method_library_;
  IBusBus* ibus_;
  IBusPanelService* ibus_panel_service_;
};

//
// cros APIs
//

// The function will be bound to chromeos::MonitorInputMethodUiStatus with
// dlsym() in load.cc so it needs to be in the C linkage, so the symbol
// name does not get mangled.
extern "C"
InputMethodUiStatusConnection* ChromeOSMonitorInputMethodUiStatus(
    const InputMethodUiStatusMonitorFunctions& monitor_functions,
    void* input_method_library) {
  DLOG(INFO) << "MonitorInputMethodUiStatus";

  InputMethodUiStatusConnection* connection =
      new InputMethodUiStatusConnection(monitor_functions,
                                        input_method_library);

  // It's totally fine if ConnectToIBus() fails here, as we'll get "connected"
  // gobject signal once the connection becomes ready.
  if (connection->ConnectToIBus()) {
    connection->MaybeRestorePanelService();
  }
  return connection;
}

extern "C"
void ChromeOSDisconnectInputMethodUiStatus(
    InputMethodUiStatusConnection* connection) {
  DLOG(INFO) << "DisconnectInputMethodUiStatus";
  delete connection;
}

extern "C"
void ChromeOSNotifyCandidateClicked(InputMethodUiStatusConnection* connection,
                                    int index, int button, int flags) {
  DLOG(INFO) << "NotifyCandidateClicked";
  DCHECK(connection);
  if (connection) {
    connection->NotifyCandidateClicked(index, button, flags);
  }
}

extern "C"
void ChromeOSNotifyCursorUp(InputMethodUiStatusConnection* connection) {
  DLOG(INFO) << "NotifyCursorUp";
  DCHECK(connection);
  if (connection) {
    connection->NotifyCursorUp();
  }
}

extern "C"
void ChromeOSNotifyCursorDown(InputMethodUiStatusConnection* connection) {
  DLOG(INFO) << "NotifyCursorDown";
  DCHECK(connection);
  if (connection) {
    connection->NotifyCursorDown();
  }
}

extern "C"
void ChromeOSNotifyPageUp(InputMethodUiStatusConnection* connection) {
  DLOG(INFO) << "NotifyPageUp";
  DCHECK(connection);
  if (connection) {
    connection->NotifyPageUp();
  }
}

extern "C"
void ChromeOSNotifyPageDown(InputMethodUiStatusConnection* connection) {
  DLOG(INFO) << "NotifyPageDown";
  DCHECK(connection);
  if (connection) {
    connection->NotifyPageDown();
  }
}

extern "C"
void ChromeOSMonitorInputMethodConnection(
    InputMethodUiStatusConnection* connection,
    InputMethodConnectionChangeMonitorFunction connection_change_handler) {
  DLOG(INFO) << "MonitorInputMethodConnection";
  DCHECK(connection);
  if (connection) {
    connection->MonitorInputMethodConnection(connection_change_handler);
  }
}

}  // namespace chromeos
