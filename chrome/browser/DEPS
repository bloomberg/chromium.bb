include_rules = [
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
  # TODO(ben): http://crbug.com/118410 will fix the following:
  "+content/browser/renderer_host/render_view_host_factory.h",
  "+content/browser/web_contents/web_contents_drag_win.h",
  "+content/browser/web_contents/web_contents_view_helper.h",
  "+content/browser/web_contents/web_drag_dest_win.h",
  # DO NOT ADD ANY MORE ITEMS TO THE ABOVE LIST!

  "-content/common",

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

  # FIXME: these should probably not be here, we need to find a better
  # structure for these includes.
  "+chrome/renderer",
]
