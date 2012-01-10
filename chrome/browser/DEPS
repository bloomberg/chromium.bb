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
  "+webkit/forms",  # Defines some types that are marshalled over IPC.
  "+webkit/glue",  # Defines some types that are marshalled over IPC.
  "+webkit/plugins",  # Defines some types that are marshalled over IPC.
  "+webkit/quota",
  "+xib_localizers", # For generated mac localization helpers

  # TODO(jam): this needs to be removed, and only use content/public. BUG=98716
  "+content/browser",
  "-content/browser/download/download_manager_impl.h",
  "-content/browser/download/download_resource_handler.h",
  "-content/browser/download/save_file_manager.h",
  "-content/browser/notification_service_impl.h",
  "-content/browser/tab_contents/navigation_controller_impl.h",
  "-content/browser/tab_contents/navigation_entry_impl.h",
  "-content/browser/plugin_service_impl.h",
  "-content/browser/worker_service_impl.h",

  "-content/common",
  # TODO(jam): remove all the exceptions. BUG=98716
  "+content/common/view_messages.h",

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
