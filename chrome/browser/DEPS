include_rules = [
  "+apps",
  "+breakpad",
  "+chrome/app",
  "+chrome/installer",
  "+chrome/tools/profiles",  # For history unit tests.
  "+chromeos",
  "+components/autofill/content/browser",
  "+components/autofill/core/browser",
  "+components/autofill/core/common",
  "+components/auto_login_parser",
  "+components/breakpad",
  "+components/browser_context_keyed_service",
  "+components/nacl/common",
  "+components/navigation_interception",
  "+components/policy",
  "+components/startup_metric_utils",
  "+components/translate/common",
  "+components/user_prefs",
  "+components/visitedlink/browser",
  "+components/webdata",
  "+components/web_contents_delegate_android",
  "+components/web_modal",
  "+content/public/browser",
  "+device/bluetooth",
  "+device/media_transfer_protocol",
  "+extensions/browser",
  "+extensions/common",
  "+google/cacheinvalidation",  # Sync invalidation API protobuf files.
  "+google_apis",
  "+google_update",
  "+grit",  # For generated headers
  "+installer_util_strings",  # For generated headers
  "+media/base",  # For media switches
  "+media/audio",
  "+policy",  # For generated headers and source
  "+ppapi/c",  # For various types.
  "+ppapi/host",
  "+ppapi/proxy",
  "+ppapi/shared_impl/api_id.h",
  # Defines some types that are marshalled over IPC.
  "+ppapi/shared_impl/ppp_flash_browser_operations_shared.h",
  "+rlz",
  "+sandbox/linux",
  "+sandbox/win/src",  # The path doesn't say it, but this is the Windows sandbox.
  "+skia/ext",
  "+skia/include",
  "+sync/api",  # Sync API files.
  "+sync/notifier",  # Sync invalidation API files.
  "+sync/protocol",  # Sync protobuf files.
  "+third_party/cros_system_api",
  "+win8/util",
  "+win8/viewer",
  "+xib_localizers", # For generated mac localization helpers

  # Unsafe to use from //chrome, see http://goo.gl/QQG5wo
  "-content/public/test/test_browser_context.h",

  # TODO(stuartmorgan): Remove these by refactoring necessary code out of
  # webkit/. See crbug.com/146251
  # DO NOT ADD ANY MORE ITEMS TO THE LIST BELOW!

  # TODO(kinuko): Remove all other webkit/* dependencies after we cleaned
  # up webkit/. crbug.com/239710
  "+webkit/browser",
  "+webkit/common",

  # webkit/glue files are listed individually since they aren't conceptually
  # grouped like the other webkit/ files, and can therefore be tackled one at
  # a time.
  "+webkit/glue/resource_type.h",
  # DO NOT ADD ANY MORE ITEMS TO THE ABOVE LIST!

  "-chrome/browser/ui/views",
  "+chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h",
  "+chrome/browser/ui/views/ash/chrome_browser_main_extra_parts_ash.h",

  # Other libraries.
  "+chrome/third_party/mozilla_security_manager",
  "+libxml",  # For search engine definition parsing.
  "+third_party/apple_sample_code",  # Apple code ImageAndTextCell.
  "+third_party/bzip2",
  "+third_party/expat",
  "+third_party/iaccessible2",
  "+third_party/icu/source/common/unicode",
  "+third_party/icu/source/i18n/unicode",
  "+third_party/isimpledom",
  "+third_party/leveldatabase",
  "+third_party/libevent",  # For the remote V8 debugging server
  "+third_party/libjingle",
  "+third_party/libyuv",
  "+third_party/protobuf/src/google/protobuf",
  "+third_party/re2",
  "+third_party/sqlite",
  "+third_party/undoview",
  "+third_party/zlib",

  # No inclusion of WebKit from the browser, other than strictly enum/POD,
  # header-only types, and some selected common code.
  "-third_party/WebKit",
  "+third_party/WebKit/public/platform/WebRect.h",
  "+third_party/WebKit/public/platform/WebReferrerPolicy.h",
  "+third_party/WebKit/public/platform/WebScreenInfo.h",
  "+third_party/WebKit/public/web/WebAutofillClient.h",
  "+third_party/WebKit/public/web/WebCache.h",
  "+third_party/WebKit/public/web/WebContextMenuData.h",
  "+third_party/WebKit/public/web/WebCursorInfo.h",
  "+third_party/WebKit/public/web/WebFindOptions.h",
  "+third_party/WebKit/public/web/WebInputEvent.h",
  "+third_party/WebKit/public/web/WebMediaPlayerAction.h",
  "+third_party/WebKit/public/web/WebNotificationPresenter.h",
  "+third_party/WebKit/public/web/WebPluginAction.h",
  "+third_party/WebKit/public/web/WebScreenInfo.h",
  "+third_party/WebKit/public/web/WebTextDirection.h",
  "+third_party/WebKit/public/web/WebWindowFeatures.h",
]
