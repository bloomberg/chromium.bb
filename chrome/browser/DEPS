include_rules = [
  "+chrome/app",
  "+chrome/app/locales",
  "+chrome/installer",
  "+chrome/personalization",
  "+chrome/profile_import",
  "+chrome/tools/profiles",  # For history unit tests.
  "+chrome/views",
  "+content/plugin/plugin_interpose_util_mac.h",
  "+content/public/browser",
  "+grit",  # For generated headers
  "+media/audio",  # For audio input for speech input feature.
  "+policy",  # For generated headers and source
  "+ppapi/c",  # For various types.
  "+ppapi/proxy",
  "+rlz",
  "+sandbox/linux",
  "+sandbox/src",  # The path doesn't say it, but this is the Windows sandbox.
  "+skia/ext",
  "+skia/include",
  "+third_party/cros",
  "+third_party/cros_system_api",
  "+webkit/database",
  "+webkit/glue",  # Defines some types that are marshalled over IPC.
  "+webkit/plugins",  # Defines some types that are marshalled over IPC.
  "+webkit/quota",
  "+xib_localizers", # For generated mac localization helpers
  
  # TODO(jam): this needs to be removed, and only use content/public. BUG=98716
  "+content/browser",
  # Temporary to avoid regressions until above is done.
  "-content/common/appcache",
  "-content/common/file_system",
  "-content/common/gpu",
  "-content/common/media",
  "-content/common/net",
  "-content/common/appcache_messages.h",
  "-content/common/child_process_messages.h",
  "-content/common/clipboard_messages.h",
  "-content/common/database_messages.h",
  "-content/common/desktop_notification_messages.h",
  "-content/common/device_orientation_messages.h",
  # TODO(pfeldman) "-content/common/dom_storage_messages.h",
  "-content/common/dom_storage_messages.h",
  "-content/common/drag_messages.h",
  "-content/common/file_system_messages.h",
  "-content/common/file_utilities_messages.h",
  "-content/common/gamepad_messages.h",
  # TODO(jam) "-content/common/geolocation_messages.h",
  "-content/common/indexed_db_messages.h",
  # TODO(jam) "-content/common/intents_messages.h",
  "-content/common/java_bridge_messages.h",
  "-content/common/mime_registry_messages.h",
  "-content/common/p2p_messages.h",
  "-content/common/pepper_file_messages.h",
  "-content/common/pepper_messages.h",
  "-content/common/plugin_messages.h",
  "-content/common/quota_messages.h",
  "-content/common/resource_messages.h",
  "-content/common/socket_stream_messages.h",
  "-content/common/speech_input_messages.h",
  "-content/common/swapped_out_messages.h",
  "-content/common/text_input_client_messages.h",
  "-content/common/utility_messages.h",
  # TODO(jam) "-content/common/view_messages.h",
  "-content/common/webblob_messages.h",
  "-content/common/worker_messages.h",

  # Other libraries.
  "+chrome/third_party/hunspell",
  "+chrome/third_party/mozilla_security_manager",
  "+libxml",  # For search engine definition parsing.
  "+third_party/apple_sample_code",  # Apple code ImageAndTextCell.
  "+third_party/cld",
  "+third_party/expat",
  "+third_party/iaccessible2",
  "+third_party/isimpledom",
  "+third_party/libevent",  # For the remote V8 debugging server
  "+third_party/libjingle",
  "+third_party/protobuf/src/google/protobuf",
  "+third_party/sqlite",
  "+third_party/undoview",
  "+v8/include",  # Browser uses V8 to get the version and run the debugger.

  # FIXME: these should probably not be here, we need to find a better
  # structure for these includes.
  "+chrome/renderer",
]
