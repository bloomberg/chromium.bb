// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ibus_engine_controller.h"

#if defined(HAVE_IBUS)
#include <ibus.h>
#endif

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"

namespace chromeos {
namespace input_method {

IBusEngineController::EngineProperty::EngineProperty()
    : sensitive(false), visible(false), type(PROPERTY_TYPE_NORMAL),
      checked(false), modified(0) {
}

IBusEngineController::EngineProperty::~EngineProperty() {
  STLDeleteContainerPointers(children.begin(), children.end());
}

#if defined(HAVE_IBUS)

#define IBUS_TYPE_CHROMEOS_ENGINE (ibus_chromeos_engine_get_type())
#define IBUS_CHROMEOS_ENGINE(obj)                                \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), IBUS_TYPE_CHROMEOS_ENGINE,  \
                              IBusChromeOSEngine))
#define IBUS_CHROMEOS_ENGINE_CLASS(klass)                        \
  (G_TYPE_CHECK_CLASS_CAST((klass), IBUS_TYPE_CHROMEOS_ENGINE,   \
                           IBusChromeOSEngineClass))
#define CHROMEOS_IS_EXTENSION_ENGINE(obj)                        \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), IBUS_TYPE_CHROMEOS_ENGINE))
#define CHROMEOS_IS_EXTENSION_ENGINE_CLASS(klass)                \
  (G_TYPE_CHECK_CLASS_TYPE((klass), IBUS_TYPE_CHROMEOS_ENGINE))
#define IBUS_CHROMEOS_ENGINE_GET_CLASS(obj)                      \
  (G_TYPE_INSTANCE_GET_CLASS((obj), IBUS_TYPE_CHROMEOS_ENGINE,   \
                             IBusChromeOSEngineClass))

class IBusEngineControllerImpl;
struct IBusChromeOSEngine {
  IBusEngine parent;
  IBusEngineControllerImpl* connection;
  IBusLookupTable* table;
};

struct IBusChromeOSEngineClass {
  IBusEngineClass parent_class;
};

G_DEFINE_TYPE(IBusChromeOSEngine, ibus_chromeos_engine, IBUS_TYPE_ENGINE)

const int kDefaultCandidatePageSize = 9;

class IBusEngineControllerImpl : public IBusEngineController {
 public:
  typedef std::map<std::string, IBusEngineControllerImpl*> ConnectionMap;
  explicit IBusEngineControllerImpl(IBusEngineController::Observer* observer)
      : observer_(observer),
        ibus_(NULL),
        active_engine_(NULL),
        preedit_text_(NULL),
        preedit_cursor_(0),
        table_visible_(false),
        cursor_visible_(false),
        vertical_(false),
        page_size_(kDefaultCandidatePageSize),
        cursor_position_(0),
        aux_text_visible_(false),
        active_(false),
        focused_(false) {
    if (!g_connections_) {
      g_connections_ = new ConnectionMap;
    }
  }

  ~IBusEngineControllerImpl() {
    for (std::set<IBusChromeOSEngine*>::iterator ix = engine_instances_.begin();
         ix != engine_instances_.end(); ++ix) {
      (*ix)->connection = NULL;
    }
    if (preedit_text_) {
      g_object_unref(preedit_text_);
      preedit_text_ = NULL;
    }
    if (ibus_) {
      DisconnectIBusSignals();
      g_object_unref(ibus_);
    }
    g_connections_->erase(engine_id_);
    ClearProperties();
    DVLOG(1) << "Removing engine: " << engine_id_;
  }

  // Initializes the object. Returns true on success.
  bool Init(const char* engine_id, const char* engine_name,
            const char* description, const char* language,
            const char* layout) {
    engine_id_ = engine_id;
    engine_name_ = engine_name;
    description_ = description;
    language_ = language;
    layout_ = layout;

    bus_name_ = "org.freedesktop.IBus.";
    bus_name_ += engine_id;

    // engine_id_ must be unique.
    if (g_connections_->find(engine_id_) != g_connections_->end()) {
      DVLOG(1) << "ibus engine name already in use: " << engine_id_;
      return false;
    }

    // Initialize an IBus bus.
    ibus_init();
    ibus_ = ibus_bus_new();

    // Check the IBus connection status.
    if (!ibus_) {
      DVLOG(1) << "ibus_bus_new() failed";
      return false;
    }

    (*g_connections_)[engine_id_] = this;
    DVLOG(1) << "Adding engine: " << engine_id_;

    // Check the IBus connection status.
    bool result = true;
    if (ibus_bus_is_connected(ibus_)) {
      DVLOG(1) << "ibus_bus_is_connected(). IBus connection is ready.";
      result = MaybeCreateComponent();
    }

    // Start listening the gobject signals regardless of the bus connection
    // status.
    ConnectIBusSignals();
    return result;
  }

  virtual void SetPreeditText(const char* text, int cursor) {
    if (active_engine_) {
      DVLOG(1) << "SetPreeditText";
      if (preedit_text_) {
        g_object_unref(preedit_text_);
        preedit_text_ = NULL;
      }
      preedit_cursor_ = cursor;
      preedit_text_ = static_cast<IBusText*>(
          g_object_ref_sink(ibus_text_new_from_string(text)));
      ibus_engine_update_preedit_text(IBUS_ENGINE(active_engine_),
                                      preedit_text_,
                                      preedit_cursor_, TRUE);
    }
  }

  virtual void SetPreeditUnderline(int start, int end, int type) {
    DVLOG(1) << "SetPreeditUnderline";
    if (active_engine_ && preedit_text_) {
      // Translate the type to ibus's constants.
      int underline_type;
      switch (type) {
        case UNDERLINE_NONE:
          underline_type = IBUS_ATTR_UNDERLINE_NONE;
          break;

        case UNDERLINE_SINGLE:
          underline_type = IBUS_ATTR_UNDERLINE_SINGLE;
          break;

        case UNDERLINE_DOUBLE:
          underline_type = IBUS_ATTR_UNDERLINE_DOUBLE;
          break;

        case UNDERLINE_LOW:
          underline_type = IBUS_ATTR_UNDERLINE_LOW;
          break;

        case UNDERLINE_ERROR:
          underline_type = IBUS_ATTR_UNDERLINE_ERROR;
          break;

        default:
          return;
      }

      ibus_text_append_attribute(preedit_text_, IBUS_ATTR_TYPE_UNDERLINE,
                                 underline_type, start, end);
      ibus_engine_update_preedit_text(IBUS_ENGINE(active_engine_),
                                      preedit_text_,
                                      preedit_cursor_, TRUE);
    }
  }

  virtual void CommitText(const char* text) {
    if (active_engine_) {
      DVLOG(1) << "CommitText";
      // Reset the preedit text when a commit occurs.
      SetPreeditText("", 0);
      if (preedit_text_) {
        g_object_unref(preedit_text_);
        preedit_text_ = NULL;
      }
      IBusText* ibus_text = static_cast<IBusText*>(
          ibus_text_new_from_string(text));
      ibus_engine_commit_text(IBUS_ENGINE(active_engine_), ibus_text);
    }
  }

  virtual void SetTableVisible(bool visible) {
    table_visible_ = visible;
    if (active_engine_) {
      DVLOG(1) << "SetTableVisible";
      ibus_engine_update_lookup_table(IBUS_ENGINE(active_engine_),
                                      active_engine_->table,
                                      table_visible_);
      if (visible) {
        ibus_engine_show_lookup_table(IBUS_ENGINE(active_engine_));
      } else {
        ibus_engine_hide_lookup_table(IBUS_ENGINE(active_engine_));
      }
    }
  }

  virtual void SetCursorVisible(bool visible) {
    cursor_visible_ = visible;
    if (active_engine_) {
      DVLOG(1) << "SetCursorVisible";
      ibus_lookup_table_set_cursor_visible(active_engine_->table, visible);
      ibus_engine_update_lookup_table(IBUS_ENGINE(active_engine_),
                                      active_engine_->table,
                                      table_visible_);
    }
  }

  virtual void SetOrientationVertical(bool vertical) {
    vertical_ = vertical;
    if (active_engine_) {
      DVLOG(1) << "SetOrientationVertical";
      ibus_lookup_table_set_orientation(active_engine_->table,
                                        vertical ? IBUS_ORIENTATION_VERTICAL :
                                        IBUS_ORIENTATION_HORIZONTAL);
      ibus_engine_update_lookup_table(IBUS_ENGINE(active_engine_),
                                      active_engine_->table,
                                      table_visible_);
    }
  }

  virtual void SetPageSize(unsigned int size) {
    page_size_ = size;
    if (active_engine_) {
      DVLOG(1) << "SetPageSize";
      ibus_lookup_table_set_page_size(active_engine_->table, size);
      ibus_engine_update_lookup_table(IBUS_ENGINE(active_engine_),
                                      active_engine_->table,
                                      table_visible_);
    }
  }

  virtual void ClearCandidates() {
    if (active_engine_) {
      DVLOG(1) << "ClearCandidates";
      ibus_lookup_table_clear(active_engine_->table);
      ibus_engine_update_lookup_table(IBUS_ENGINE(active_engine_),
                                      active_engine_->table,
                                      table_visible_);
    }
  }

  virtual void SetCandidates(std::vector<Candidate> candidates) {
    // Text with this foreground color will be treated as an annotation.
    const guint kAnnotationForegroundColor = 0x888888;
    if (active_engine_) {
      DVLOG(1) << "SetCandidates";
      ibus_lookup_table_clear(active_engine_->table);

      for (std::vector<Candidate>::iterator ix = candidates.begin();
           ix != candidates.end(); ++ix ) {
        if (!ix->annotation.empty()) {
          // If there's an annotation, append it to the value.
          std::string candidate(ix->value);
          candidate += " ";
          size_t start = g_utf8_strlen(candidate.c_str(), -1);
          candidate += ix->annotation;

          // Set the foreground color of the annotation text so that the
          // candidate window will treat it properly.
          size_t end = start + g_utf8_strlen(ix->annotation.c_str(), -1);
          IBusText* ibus_text = static_cast<IBusText*>(
              ibus_text_new_from_string(candidate.c_str()));
          ibus_text_append_attribute(ibus_text,
                                     IBUS_ATTR_TYPE_FOREGROUND,
                                     kAnnotationForegroundColor,
                                     start,
                                     end);
          ibus_lookup_table_append_candidate(active_engine_->table, ibus_text);
        } else {
          IBusText* ibus_text = static_cast<IBusText*>(
              ibus_text_new_from_string(ix->value.c_str()));
          ibus_lookup_table_append_candidate(active_engine_->table, ibus_text);
        }

        // Add the label if it's set.
        if (!ix->label.empty()) {
          IBusText* ibus_label = static_cast<IBusText*>(
              ibus_text_new_from_string(ix->label.c_str()));
          ibus_lookup_table_set_label(active_engine_->table,
                                      std::distance(candidates.begin(), ix),
                                      ibus_label);
        }
      }
      ibus_engine_update_lookup_table(IBUS_ENGINE(active_engine_),
                                      active_engine_->table,
                                      table_visible_);
    }
  }

  virtual void SetCandidateAuxText(const char* text) {
    aux_text_ = text;
    if (active_engine_) {
      DVLOG(1) << "SetCandidateAuxText";
      IBusText* ibus_text = static_cast<IBusText*>(
          ibus_text_new_from_string(aux_text_.c_str()));
      ibus_engine_update_auxiliary_text(IBUS_ENGINE(active_engine_), ibus_text,
                                        aux_text_visible_);
      ibus_engine_update_lookup_table(IBUS_ENGINE(active_engine_),
                                      active_engine_->table,
                                      table_visible_);
    }
  }

  virtual void SetCandidateAuxTextVisible(bool visible) {
    aux_text_visible_ = visible;
    if (active_engine_) {
      DVLOG(1) << "SetCandidateAuxTextVisible";
      IBusText* ibus_text = static_cast<IBusText*>(
          ibus_text_new_from_string(aux_text_.c_str()));
      ibus_engine_update_auxiliary_text(IBUS_ENGINE(active_engine_), ibus_text,
                                        aux_text_visible_);
      ibus_engine_update_lookup_table(IBUS_ENGINE(active_engine_),
                                      active_engine_->table,
                                      table_visible_);
    }
  }

  virtual void SetCursorPosition(unsigned int position) {
    cursor_position_ = position;
    if (active_engine_) {
      DVLOG(1) << "SetCursorPosition";
      ibus_lookup_table_set_cursor_pos(active_engine_->table, position);
      ibus_engine_update_lookup_table(IBUS_ENGINE(active_engine_),
                                      active_engine_->table,
                                      table_visible_);
    }
  }

  virtual bool RegisterProperties(
      const std::vector<EngineProperty*>& properties) {
    DVLOG(1) << "RegisterProperties";
    ClearProperties();
    CopyProperties(properties, &properties_);

    if (active_engine_) {
      if (!SetProperties()) {
        return false;
      }
    }
    return true;
  }

  void ClearProperties() {
    STLDeleteContainerPointers(properties_.begin(), properties_.end());
    properties_.clear();
    property_map_.clear();
  }

  virtual bool UpdateProperties(
      const std::vector<EngineProperty*>& properties) {
    DVLOG(1) << "UpdateProperties";
    return UpdatePropertyList(properties);
  }

  void CopyProperties(const std::vector<EngineProperty*>& properties,
                      std::vector<EngineProperty*>* dest) {
    for (std::vector<EngineProperty*>::const_iterator property =
             properties.begin(); property != properties.end(); ++property) {
      if (property_map_.find((*property)->key) != property_map_.end()) {
        DVLOG(1) << "Property collision on name: " << (*property)->key;
        return;
      }

      EngineProperty* new_property = new EngineProperty;
      dest->push_back(new_property);
      property_map_[(*property)->key] = new_property;

      new_property->key = (*property)->key;
      new_property->label = (*property)->label;
      new_property->tooltip = (*property)->tooltip;
      new_property->sensitive = (*property)->sensitive;
      new_property->visible = (*property)->visible;
      new_property->type = (*property)->type;
      new_property->checked = (*property)->checked;

      CopyProperties((*property)->children, &(new_property->children));
    }
  }

  IBusPropList *MakePropertyList(
      const std::vector<EngineProperty*>& properties) {
    IBusPropList *prop_list = ibus_prop_list_new();
    for (std::vector<EngineProperty*>::const_iterator property =
             properties.begin(); property != properties.end(); ++property) {
      IBusPropList *children = NULL;
      if (!(*property)->children.empty()) {
        children = MakePropertyList((*property)->children);
      }

      IBusPropType type = PROP_TYPE_NORMAL;
      switch ((*property)->type) {
        case PROPERTY_TYPE_NORMAL:
          type = PROP_TYPE_NORMAL;
          break;
        case PROPERTY_TYPE_TOGGLE:
          type = PROP_TYPE_TOGGLE;
          break;
        case PROPERTY_TYPE_RADIO:
          type = PROP_TYPE_RADIO;
          break;
        case PROPERTY_TYPE_SEPARATOR:
          type = PROP_TYPE_SEPARATOR;
          break;
        case PROPERTY_TYPE_MENU:
          type = PROP_TYPE_MENU;
          break;
        default:
          NOTREACHED();
          break;
      }

      IBusPropState state = (*property)->checked ? PROP_STATE_CHECKED :
                                                   PROP_STATE_UNCHECKED;

      IBusProperty *ibus_property =
          ibus_property_new((*property)->key.c_str(),
                            type,
                            static_cast<IBusText*>(
                                ibus_text_new_from_string(
                                    (*property)->label.c_str())),
                            NULL,
                            static_cast<IBusText*>(
                                ibus_text_new_from_string(
                                    (*property)->tooltip.c_str())),
                            (*property)->sensitive,
                            (*property)->visible,
                            state,
                            children);

      ibus_prop_list_append(prop_list, ibus_property);
    }
    return prop_list;
  }

  bool SetProperties() {
    IBusPropList *prop_list = MakePropertyList(properties_);
    ibus_engine_register_properties(IBUS_ENGINE(active_engine_), prop_list);
    return true;
  }

  bool UpdatePropertyList(const std::vector<EngineProperty*>& properties) {
    for (std::vector<EngineProperty*>::const_iterator property =
             properties.begin(); property != properties.end(); ++property) {
      std::map<std::string, EngineProperty*>::iterator cur_property =
          property_map_.find((*property)->key);
      if (cur_property == property_map_.end()) {
        DVLOG(1) << "Missing property: " << (*property)->key;
        return false;
      }

      if ((*property)->modified & PROPERTY_MODIFIED_LABEL) {
        cur_property->second->label = (*property)->label;
      }
      if ((*property)->modified & PROPERTY_MODIFIED_TOOLTIP) {
        cur_property->second->tooltip = (*property)->tooltip;
      }
      if ((*property)->modified & PROPERTY_MODIFIED_SENSITIVE) {
        cur_property->second->sensitive = (*property)->sensitive;
      }
      if ((*property)->modified & PROPERTY_MODIFIED_VISIBLE) {
        cur_property->second->visible = (*property)->visible;
      }
      if ((*property)->modified & PROPERTY_MODIFIED_TYPE) {
        cur_property->second->type = (*property)->type;
      }
      if ((*property)->modified & PROPERTY_MODIFIED_CHECKED) {
        cur_property->second->checked = (*property)->checked;
      }

      if (active_engine_) {
        IBusPropType type = PROP_TYPE_NORMAL;
        switch (cur_property->second->type) {
          case PROPERTY_TYPE_NORMAL:
            type = PROP_TYPE_NORMAL;
            break;
          case PROPERTY_TYPE_TOGGLE:
            type = PROP_TYPE_TOGGLE;
            break;
          case PROPERTY_TYPE_RADIO:
            type = PROP_TYPE_RADIO;
            break;
          case PROPERTY_TYPE_SEPARATOR:
            type = PROP_TYPE_SEPARATOR;
            break;
          case PROPERTY_TYPE_MENU:
            type = PROP_TYPE_MENU;
            break;
          default:
            NOTREACHED();
            break;
        }

        IBusPropState state = cur_property->second->checked ?
            PROP_STATE_CHECKED : PROP_STATE_UNCHECKED;

        IBusProperty *ibus_property =
            ibus_property_new(cur_property->second->key.c_str(),
                              type,
                              static_cast<IBusText*>(
                                  ibus_text_new_from_string(
                                      cur_property->second->label.c_str())),
                              NULL,
                              static_cast<IBusText*>(
                                  ibus_text_new_from_string(
                                      cur_property->second->tooltip.c_str())),
                              cur_property->second->sensitive,
                              cur_property->second->visible,
                              state,
                              NULL);

        ibus_engine_update_property(IBUS_ENGINE(active_engine_), ibus_property);
      }
      if (!UpdatePropertyList((*property)->children)) {
        return false;
      }
    }
    return true;
  }

  virtual void KeyEventDone(KeyEventHandle* key_data, bool handled) {
    GDBusMethodInvocation* invocation =
        reinterpret_cast<GDBusMethodInvocation*>(key_data);

    g_dbus_method_invocation_return_value(invocation,
                                          g_variant_new("(b)", handled));
  }


  static void InitEngineClass(IBusChromeOSEngineClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS(klass);
    IBusEngineClass *engine_class = IBUS_ENGINE_CLASS(klass);

    object_class->constructor = EngineConstructor;
    ibus_object_class->destroy = (IBusObjectDestroyFunc) OnDestroy;

    IBUS_SERVICE_CLASS(klass)->service_method_call = OnServiceMethodCall;

    engine_class->process_key_event = NULL;
    engine_class->reset = OnReset;
    engine_class->enable = OnEnable;
    engine_class->disable = OnDisable;
    engine_class->focus_in = OnFocusIn;
    engine_class->focus_out = OnFocusOut;
    engine_class->page_up = OnPageUp;
    engine_class->page_down = OnPageDown;
    engine_class->cursor_up = OnCursorUp;
    engine_class->cursor_down = OnCursorDown;
    engine_class->property_activate = OnPropertyActivate;
    engine_class->candidate_clicked = OnCandidateClicked;
  }

  static void InitEngine(IBusChromeOSEngine* engine) {
  }

 private:
  // Installs gobject signal handlers to |ibus_|.
  void ConnectIBusSignals() {
    if (!ibus_) {
      return;
    }
    DVLOG(1) << "ConnectIBusSignals";
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

  // Handles "connected" signal from ibus-daemon.
  static void IBusBusConnectedCallback(IBusBus* bus, gpointer user_data) {
    DVLOG(1) << "IBus connection is recovered.";
    g_return_if_fail(user_data);
    IBusEngineControllerImpl* self
        = static_cast<IBusEngineControllerImpl*>(user_data);
    DCHECK(ibus_bus_is_connected(self->ibus_));
    if (!self->MaybeCreateComponent()) {
      DVLOG(1) << "MaybeCreateComponent() failed";
      return;
    }
  }

  // Handles "disconnected" signal from ibus-daemon.
  static void IBusBusDisconnectedCallback(IBusBus* bus, gpointer user_data) {
    DVLOG(1) << "IBus connection is terminated.";
    if (g_factory_) {
      g_object_unref(g_factory_);
      g_factory_ = NULL;
    }
  }

  bool MaybeCreateComponent() {
    if (!ibus_ || !ibus_bus_is_connected(ibus_)) {
      return false;
    }

    DVLOG(1) << "MaybeCreateComponent";
    IBusComponent* component = ibus_component_new(bus_name_.c_str(),
                                                  description_.c_str(),
                                                  "",
                                                  "",
                                                  engine_name_.c_str(),
                                                  "",
                                                  "",
                                                  "");
    g_object_ref_sink(component);
    ibus_component_add_engine(component,
                              ibus_engine_desc_new(engine_id_.c_str(),
                                                   description_.c_str(),
                                                   description_.c_str(),
                                                   language_.c_str(),
                                                   "",
                                                   engine_name_.c_str(),
                                                   "",
                                                   layout_.c_str()));

    if (!g_factory_) {
      g_factory_ = ibus_factory_new(ibus_bus_get_connection(ibus_));
      g_object_ref_sink(g_factory_);
    }

    ibus_factory_add_engine(g_factory_, engine_id_.c_str(),
                            IBUS_TYPE_CHROMEOS_ENGINE);
    ibus_bus_register_component(ibus_, component);
    g_object_unref(component);

    return true;
  }

  static void OnServiceMethodCall(IBusService* service,
                                  GDBusConnection* connection,
                                  const gchar* sender,
                                  const gchar* object_path,
                                  const gchar* interface_name,
                                  const gchar* method_name,
                                  GVariant* parameters,
                                  GDBusMethodInvocation* invocation) {
    if (g_strcmp0(method_name, "ProcessKeyEvent") == 0) {
      // Override the default ProcessKeyEvent handler so that we can send the
      // response asynchronously.
      IBusEngine *engine = IBUS_ENGINE(service);

      guint keyval;
      guint keycode;
      guint state;
      g_variant_get(parameters, "(uuu)", &keyval, &keycode, &state);

      OnProcessKeyEvent(engine, keyval, keycode, state, invocation);
    } else {
      IBUS_SERVICE_CLASS(
          ibus_chromeos_engine_parent_class)->service_method_call(
              service,
              connection,
              sender,
              object_path,
              interface_name,
              method_name,
              parameters,
              invocation);
    }
  }

  static void OnProcessKeyEvent(IBusEngine* ibus_engine, guint keyval,
                                guint keycode, guint modifiers,
                                GDBusMethodInvocation* key_data) {
    DVLOG(1) << "OnProcessKeyEvent";
    IBusChromeOSEngine* engine = IBUS_CHROMEOS_ENGINE(ibus_engine);
    if (engine->connection) {
      engine->connection->observer_->OnKeyEvent(
          !(modifiers & IBUS_RELEASE_MASK),
          keyval, keycode,
          modifiers & IBUS_MOD1_MASK,
          modifiers & IBUS_CONTROL_MASK,
          modifiers & IBUS_SHIFT_MASK,
          reinterpret_cast<KeyEventHandle*>(key_data));
    }
  }

  static void OnReset(IBusEngine* ibus_engine) {
    DVLOG(1) << "OnReset";
    IBusChromeOSEngine* engine = IBUS_CHROMEOS_ENGINE(ibus_engine);
    if (engine->connection) {
      engine->connection->observer_->OnReset();
    }
  }

  static void OnEnable(IBusEngine* ibus_engine) {
    DVLOG(1) << "OnEnable";
    IBusChromeOSEngine* engine = IBUS_CHROMEOS_ENGINE(ibus_engine);
    if (engine->connection) {
      engine->connection->active_engine_ = engine;
      engine->connection->active_ = true;
      engine->connection->observer_->OnEnable();
      if (engine->connection->focused_) {
        engine->connection->observer_->OnFocusIn();
      }
      engine->connection->SetProperties();
    }
  }

  static void OnDisable(IBusEngine* ibus_engine) {
    DVLOG(1) << "OnDisable";
    IBusChromeOSEngine* engine = IBUS_CHROMEOS_ENGINE(ibus_engine);
    if (engine->connection) {
      engine->connection->active_ = false;
      engine->connection->observer_->OnDisable();
      if (engine->connection->active_engine_ == engine) {
        engine->connection->active_engine_ = NULL;
      }
    }
  }

  static void OnFocusIn(IBusEngine* ibus_engine) {
    DVLOG(1) << "OnFocusIn";
    IBusChromeOSEngine* engine = IBUS_CHROMEOS_ENGINE(ibus_engine);
    if (engine->connection) {
      engine->connection->focused_ = true;
      if (engine->connection->active_) {
        engine->connection->observer_->OnFocusIn();
      }
      engine->connection->SetProperties();
    }
  }

  static void OnFocusOut(IBusEngine* ibus_engine) {
    DVLOG(1) << "OnFocusOut";
    IBusChromeOSEngine* engine = IBUS_CHROMEOS_ENGINE(ibus_engine);
    if (engine->connection) {
      engine->connection->focused_ = false;
      if (engine->connection->active_) {
        engine->connection->observer_->OnFocusOut();
      }
    }
  }

  static void OnPageUp(IBusEngine* ibus_engine) {
    // Not implemented
  }

  static void OnPageDown(IBusEngine* ibus_engine) {
    // Not implemented
  }

  static void OnCursorUp(IBusEngine* ibus_engine) {
    // Not implemented
  }

  static void OnCursorDown(IBusEngine* ibus_engine) {
    // Not implemented
  }

  static void OnPropertyActivate(IBusEngine* ibus_engine,
                                 const gchar *prop_name, guint prop_state) {
    DVLOG(1) << "OnPropertyActivate";
    IBusChromeOSEngine* engine = IBUS_CHROMEOS_ENGINE(ibus_engine);
    if (engine->connection) {
      engine->connection->observer_->OnPropertyActivate(prop_name, prop_state);
    }
  }

  static void OnCandidateClicked(IBusEngine* ibus_engine, guint index,
                                 guint button, guint state) {
    DVLOG(1) << "OnCandidateClicked";
    IBusChromeOSEngine* engine = IBUS_CHROMEOS_ENGINE(ibus_engine);
    if (engine->connection) {
      int pressed_button = 0;
      if (button & IBUS_BUTTON1_MASK) {
        pressed_button |= MOUSE_BUTTON_1_MASK;
      }
      if (button & IBUS_BUTTON2_MASK) {
        pressed_button |= MOUSE_BUTTON_2_MASK;
      }
      if (button & IBUS_BUTTON3_MASK) {
        pressed_button |= MOUSE_BUTTON_3_MASK;
      }
      engine->connection->observer_->OnCandidateClicked(index, pressed_button,
                                                        state);
    }
  }

  static GObject* EngineConstructor(GType type, guint n_construct_params,
                                    GObjectConstructParam *construct_params) {
    IBusChromeOSEngine* engine = (IBusChromeOSEngine*)
        G_OBJECT_CLASS(ibus_chromeos_engine_parent_class)
        ->constructor(type, n_construct_params, construct_params);
    const gchar* name = ibus_engine_get_name((IBusEngine*)engine);
    ConnectionMap::iterator connection = g_connections_->find(name);
    if (connection == g_connections_->end()) {
      DVLOG(1) << "Connection never created: " << name;
      engine->connection = NULL;
      engine->table = NULL;
      return (GObject *) engine;
    }

    connection->second->engine_instances_.insert(engine);

    if (!connection->second->active_engine_) {
      connection->second->active_engine_ = engine;
    }

    engine->connection = connection->second;

    engine->table = ibus_lookup_table_new(connection->second->page_size_,
                                          connection->second->cursor_position_,
                                          connection->second->cursor_visible_,
                                          false);  // table_round
    g_object_ref_sink(engine->table);
    ibus_lookup_table_set_orientation(engine->table,
                                      connection->second->vertical_ ?
                                        IBUS_ORIENTATION_VERTICAL :
                                        IBUS_ORIENTATION_HORIZONTAL);
    ibus_engine_update_lookup_table(IBUS_ENGINE(engine), engine->table,
                                    connection->second->table_visible_);


    connection->second->SetProperties();

    return (GObject *) engine;
  }

  static void OnDestroy(IBusChromeOSEngine* chromeos_engine) {
    const gchar* name = ibus_engine_get_name((IBusEngine*)chromeos_engine);
    ConnectionMap::iterator connection = g_connections_->find(name);
    if (connection == g_connections_->end()) {
      DVLOG(1) << "Connection already destroyed, or never created: " << name;
    } else {
      connection->second->engine_instances_.erase(chromeos_engine);
      if (connection->second->active_engine_ == chromeos_engine) {
        connection->second->active_engine_ = NULL;
      }
    }
    if (chromeos_engine->table) {
      g_object_unref(chromeos_engine->table);
      chromeos_engine->table = NULL;
    }
    if (ibus_bus_is_connected(chromeos_engine->connection->ibus_)) {
      // We can't call destroy function without ibus-daemon connection,
      // otherwise browser goes into deadlock state.
      // TODO(nona): investigate the reason of dead-lock.
      IBUS_OBJECT_CLASS(ibus_chromeos_engine_parent_class)
         ->destroy(IBUS_OBJECT(chromeos_engine));
    }
  }

  IBusEngineController::Observer* observer_;
  IBusBus* ibus_;
  IBusChromeOSEngine* active_engine_;

  std::string engine_id_;
  std::string engine_name_;
  std::string description_;
  std::string language_;
  std::string layout_;
  std::string bus_name_;

  IBusText* preedit_text_;
  int preedit_cursor_;
  bool table_visible_;
  bool cursor_visible_;
  bool vertical_;
  unsigned int page_size_;
  unsigned int cursor_position_;
  std::string aux_text_;
  bool aux_text_visible_;
  std::vector<EngineProperty*> properties_;
  std::map<std::string, EngineProperty*> property_map_;

  bool active_;
  bool focused_;

  std::set<IBusChromeOSEngine*> engine_instances_;

  static ConnectionMap* g_connections_;
  static IBusFactory* g_factory_;
};

IBusEngineControllerImpl::ConnectionMap*
    IBusEngineControllerImpl::g_connections_ = NULL;

IBusFactory* IBusEngineControllerImpl::g_factory_ = NULL;

void ibus_chromeos_engine_class_init(IBusChromeOSEngineClass *klass) {
  IBusEngineControllerImpl::InitEngineClass(klass);
}

void ibus_chromeos_engine_init(IBusChromeOSEngine* engine) {
  IBusEngineControllerImpl::InitEngine(engine);
}

#endif // HAVE_IBUS

// static
IBusEngineController* IBusEngineController::Create(Observer* observer,
                                                   const char* engine_id,
                                                   const char* engine_name,
                                                   const char* description,
                                                   const char* language,
                                                   const char* layout) {
#if defined(HAVE_IBUS)
  IBusEngineControllerImpl* connection = new IBusEngineControllerImpl(observer);
  if (!connection->Init(engine_id, engine_name, description, language,
                        layout)) {
    DVLOG(1) << "Failed to Init() IBusEngineControllerImpl. "
             << "Returning NULL";
    delete connection;
    connection = NULL;
  }
  return connection;
#else
  return NULL;
#endif
}

IBusEngineController::~IBusEngineController() {
}

}  // namespace input_method
}  // namespace chromeos
