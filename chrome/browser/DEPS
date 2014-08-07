include_rules = [
  "+apps",
  "+ash",
  "+chrome/app",
  "+chrome/installer",
  "+chrome_elf/blacklist",
  "+chrome_elf/chrome_elf_constants.h",
  "+chrome_elf/create_file",
  "+chrome_elf/dll_hash",
  "+chromeos",
  "+components/autocomplete",
  "+components/autofill/content/browser",
  "+components/autofill/content/common",
  "+components/autofill/core/browser",
  "+components/autofill/core/common",
  "+components/bookmarks/browser",
  "+components/bookmarks/common",
  "+components/bookmarks/test",
  "+components/breakpad",
  "+components/cdm/browser",
  "+components/cloud_devices/common",
  "+components/content_settings",
  "+components/copresence",
  "+components/data_reduction_proxy",
  "+components/dom_distiller",
  "+components/domain_reliability",
  "+components/favicon_base",
  "+components/feedback",
  "+components/gcm_driver",
  "+components/google",
  "+components/history",
  "+components/infobars",
  "+components/invalidation",
  "+components/keyed_service",
  "+components/language_usage_metrics",
  "+components/leveldb_proto",
  "+components/metrics",
  "+components/nacl/browser",
  "+components/nacl/common",
  "+components/navigation_interception",
  "+components/navigation_metrics",
  "+components/network_time",
  "+components/omaha_query_params",
  "+components/os_crypt",
  "+components/password_manager",
  "+components/policy",
  "+components/precache",
  "+components/pref_registry",
  "+components/query_parser",
  "+components/rappor",
  "+components/renderer_context_menu",
  "+components/search",
  "+components/search_engines",
  "+components/search_provider_logos",
  "+components/session_manager",
  "+components/sessions",
  "+components/signin",
  "+components/startup_metric_utils",
  "+components/storage_monitor",
  "+components/suggestions",
  "+components/sync_driver",
  "+components/translate/content/browser",
  "+components/translate/content/common",
  "+components/translate/core/browser",
  "+components/translate/core/common",
  "+components/url_matcher",
  "+components/user_prefs",
  "+components/web_modal",
  "+components/webdata/common",
  "+content/public/browser",
  "+content/test/net",
  "+courgette",
  "+device/bluetooth",
  "+device/media_transfer_protocol",
  "+extensions/browser",
  "+extensions/common",
  "+google/cacheinvalidation",  # Sync invalidation API protobuf files.
  "+google_apis",
  "+google_update",
  "+grit",  # For generated headers
  "+installer_util_strings",  # For generated headers
  "+jni",
  "+media/base",  # For media switches
  "+policy",  # For generated headers and source
  "+ppapi/c",  # For various types.
  "+ppapi/host",
  "+ppapi/proxy",
  "+ppapi/shared_impl",
  # Defines some types that are marshalled over IPC.
  "+ppapi/shared_impl/ppp_flash_browser_operations_shared.h",
  "+rlz",
  "+sandbox/win/src",  # The path doesn't say it, but this is the Windows sandbox.
  "+skia/ext",
  "+sync/api",  # Sync API files.
  "+sync/internal_api/public/attachments",  # Needed for tests.
  "+sync/protocol",  # Sync protobuf files.
  "+third_party/cros_system_api",
  "+win8/util",

  # chrome only needs switches from cc. All usage of the compositor is from
  # content. Definitely don't include generic stuff from cc/base here, if this
  # is needed these files need to move to base/
  "+cc/base/switches.h",

  # Unsafe to use from //chrome, see http://goo.gl/QQG5wo
  "-content/public/test/test_browser_context.h",

  # TODO(stuartmorgan): Remove these by refactoring necessary code out of
  # webkit/. See crbug.com/146251
  # DO NOT ADD ANY MORE ITEMS TO THE LIST BELOW!

  # TODO(kinuko): Remove all other webkit/* dependencies after we cleaned
  # up webkit/. crbug.com/239710
  "+webkit/browser",
  "+webkit/common",

  "-chrome/browser/ui/views",
  "+chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h",
  "+chrome/browser/ui/views/ash/chrome_browser_main_extra_parts_ash.h",

  # Other libraries.
  "+libxml",  # For search engine definition parsing.
  "+third_party/google_toolbox_for_mac/src",
  "+third_party/icu/source/common/unicode",
  "+third_party/icu/source/i18n/unicode",
  "+third_party/leveldatabase",
  "+third_party/mozilla",     # Mozilla interface headers.
  "+third_party/protobuf/src/google/protobuf",
  "+third_party/re2",
  "+third_party/smhasher",
  "+third_party/sqlite",
  "+third_party/zlib",

  # No inclusion of WebKit from the browser, other than strictly enum/POD,
  # header-only types, and some selected common code.
  "-third_party/WebKit",
  "+third_party/WebKit/public/platform/WebCursorInfo.h",
  "+third_party/WebKit/public/platform/WebNotificationPermission.h",
  "+third_party/WebKit/public/platform/WebRect.h",
  "+third_party/WebKit/public/platform/WebReferrerPolicy.h",
  "+third_party/WebKit/public/platform/WebScreenInfo.h",
  "+third_party/WebKit/public/platform/WebScreenInfo.h",
  "+third_party/WebKit/public/web/WebCache.h",
  "+third_party/WebKit/public/web/WebContextMenuData.h",
  "+third_party/WebKit/public/web/WebFindOptions.h",
  "+third_party/WebKit/public/web/WebInputEvent.h",
  "+third_party/WebKit/public/web/WebMediaPlayerAction.h",
  "+third_party/WebKit/public/web/WebPageVisibilityState.h",
  "+third_party/WebKit/public/web/WebPluginAction.h",
  "+third_party/WebKit/public/web/WebTextDirection.h",
  "+third_party/WebKit/public/web/WebWindowFeatures.h",
]
