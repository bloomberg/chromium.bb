include_rules = [
  "+breakpad",
  "+chrome/app",
  "+chrome/app/locales",
  "+chrome/installer",
  "+chrome/personalization",
  "+chrome/profile_import",
  "+chrome/tools/profiles",  # For history unit tests.
  "+chrome/views",
  "+chromeos",
  "+content/plugin/plugin_interpose_util_mac.h",
  "+content/public/browser",
  "+google_update",
  "+grit",  # For generated headers
  "+policy",  # For generated headers and source
  "+ppapi/c",  # For various types.
  "+ppapi/proxy",
  "+ppapi/shared_impl/api_id.h",
  "+rlz",
  "+sandbox/linux",
  "+sandbox/src",  # The path doesn't say it, but this is the Windows sandbox.
  "+skia/ext",
  "+skia/include",
  "+sync/api",  # Sync API files.
  "+sync/protocol",  # Sync protobuf files.
  "+third_party/cros",
  "+third_party/cros_system_api",
  "+webkit/database",
  "+webkit/dom_storage",  # Defines an ENABLE_NEW_DOM_STORAGE_BACKEND flag.
  "+webkit/forms",  # Defines some types that are marshalled over IPC.
  "+webkit/glue",  # Defines some types that are marshalled over IPC.
  "+webkit/plugins",  # Defines some types that are marshalled over IPC.
  "+webkit/quota",
  "+xib_localizers", # For generated mac localization helpers

  "-content/browser",
  # TODO(jam): Need to remove all these and use only content/public. BUG=98716
  # DO NOT ADD ANY MORE ITEMS TO THE LIST BELOW!
  "+content/browser/geolocation/wifi_data_provider_common.h",
  # DO NOT ADD ANY MORE ITEMS TO THE ABOVE LIST!

  "-content/common",

  "-chrome/browser/ui/views",
  # TODO(tfarina): Remove all these. crbug.com/125846.
  # DO NOT ADD ANY MORE ITEMS TO THE LIST BELOW!
  "+chrome/browser/ui/views/ash/brightness_controller_chromeos.h",
  "+chrome/browser/ui/views/ash/caps_lock_handler.h",
  "+chrome/browser/ui/views/ash/chrome_shell_delegate.h",
  "+chrome/browser/ui/views/ash/ime_controller_chromeos.h",
  "+chrome/browser/ui/views/ash/key_rewriter.h",
  "+chrome/browser/ui/views/ash/launcher/chrome_launcher_controller.h",
  "+chrome/browser/ui/views/ash/panel_view_aura.h",
  "+chrome/browser/ui/views/ash/screenshot_taker.h",
  "+chrome/browser/ui/views/ash/user_gesture_handler.h",
  "+chrome/browser/ui/views/ash/volume_controller_chromeos.h",
  "+chrome/browser/ui/views/bookmarks/bookmark_bar_view.h",
  "+chrome/browser/ui/views/browser_actions_container.h",
  "+chrome/browser/ui/views/chrome_views_delegate.h",
  "+chrome/browser/ui/views/constrained_window_views.h",
  "+chrome/browser/ui/views/event_utils.h",
  "+chrome/browser/ui/views/extensions/extension_dialog.h",
  "+chrome/browser/ui/views/extensions/extension_dialog_observer.h",
  "+chrome/browser/ui/views/extensions/extension_popup.h",
  "+chrome/browser/ui/views/extensions/extension_view.h",
  "+chrome/browser/ui/views/frame/browser_view.h",
  "+chrome/browser/ui/views/infobars/infobar_container_view.h",
  "+chrome/browser/ui/views/location_bar/location_bar_view.h",
  "+chrome/browser/ui/views/location_bar/location_icon_view.h",
  "+chrome/browser/ui/views/notifications/balloon_view.h",
  "+chrome/browser/ui/views/notifications/balloon_view_host.h",
  "+chrome/browser/ui/views/page_info_bubble_view.h",
  "+chrome/browser/ui/views/reload_button.h",
  "+chrome/browser/ui/views/select_file_dialog_extension.h",
  "+chrome/browser/ui/views/tab_contents/render_view_context_menu_views.h",
  "+chrome/browser/ui/views/toolbar_view.h",
  "+chrome/browser/ui/views/unhandled_keyboard_event_handler.h",
  "+chrome/browser/ui/views/web_dialog_view.h",
  # DO NOT ADD ANY MORE ITEMS TO THE ABOVE LIST!

  # Other libraries.
  "+chrome/third_party/mozilla_security_manager",
  "+libxml",  # For search engine definition parsing.
  "+third_party/apple_sample_code",  # Apple code ImageAndTextCell.
  "+third_party/cld",
  "+third_party/expat",
  "+third_party/iaccessible2",
  "+third_party/icu",
  "+third_party/isimpledom",
  "+third_party/leveldatabase",
  "+third_party/libevent",  # For the remote V8 debugging server
  "+third_party/libjingle",
  "+third_party/protobuf/src/google/protobuf",
  "+third_party/sqlite",
  "+third_party/undoview",

  # FIXME: these should probably not be here, we need to find a better
  # structure for these includes.
  "+chrome/renderer",
]
