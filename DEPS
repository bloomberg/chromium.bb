# This file is used to manage the dependencies of the Chromium src repo. It is
# used by gclient to determine what version of each dependency to check out, and
# where.
#
# For more information, please refer to the official documentation:
#   https://sites.google.com/a/chromium.org/dev/developers/how-tos/get-the-code
#
# When adding a new dependency, please update the top-level .gitignore file
# to list the dependency's destination directory.
#
# -----------------------------------------------------------------------------
# Rolling deps
# -----------------------------------------------------------------------------
# All repositories in this file are git-based, using Chromium git mirrors where
# necessary (e.g., a git mirror is used when the source project is SVN-based).
# To update the revision that Chromium pulls for a given dependency:
#
#  # Create and switch to a new branch
#  git new-branch depsroll
#  # Run roll-dep (provided by depot_tools) giving the dep's path and optionally
#  # a regex that will match the line in this file that contains the current
#  # revision. The script ALWAYS rolls the dependency to the latest revision
#  # in origin/master. The path for the dep should start with src/.
#  roll-dep src/third_party/foo_package/src foo_package.git
#  # You should now have a modified DEPS file; commit and upload as normal
#  git commit -a
#  git cl upload

gclient_gn_args_file = 'src/build/config/gclient_args.gni'
gclient_gn_args = [
  'checkout_libaom',
  'checkout_nacl',
]


vars = {
  # By default, we should check out everything needed to run on the main
  # chromium waterfalls. This var can be also be set to "small", in order
  # to skip things are not strictly needed to build chromium for development
  # purposes.
  'checkout_configuration': 'default',

  # Check out and download nacl by default. This can be disabled e.g. with
  # custom_vars.
  'checkout_nacl': True,

  # By default, do not check out src-internal. This can be overridden e.g. with
  # custom_vars.
  'checkout_src_internal': False,

  # Fetch the additional packages and files needed to run all of the
  # telemetry tests. This is false by default as some stuff is only
  # privately accessible.
  'checkout_telemetry_dependencies': False,

  # libaom provides support for AV1 but the bitstream is not frozen.
  'checkout_libaom': False,

  # TODO(dpranke): change to != "small" once != is supported.
  'checkout_traffic_annotation_tools': 'checkout_configuration == "default"',
  'checkout_instrumented_libraries': 'checkout_linux and checkout_configuration == "default"',

  'aomedia_git': 'https://aomedia.googlesource.com',
  'chromium_git': 'https://chromium.googlesource.com',
  'swiftshader_git': 'https://swiftshader.googlesource.com',
  'pdfium_git': 'https://pdfium.googlesource.com',
  'boringssl_git': 'https://boringssl.googlesource.com',
  'skia_git': 'https://skia.googlesource.com',
  'webrtc_git': 'https://webrtc.googlesource.com',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling sfntly
  # and whatever else without interference from each other.
  'sfntly_revision': '2439bd08ff93d4dce761dd6b825917938bd35a4f',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling Skia
  # and whatever else without interference from each other.
  'skia_revision': 'd1f5b4908fba8218cb9de57ec440befaf1fa7a1d',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling V8
  # and whatever else without interference from each other.
  'v8_revision': 'c93158d2347dcab8e7ecdbe3af2749ffec02c7d1',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling swarming_client
  # and whatever else without interference from each other.
  'swarming_revision': '6fd3c7b6eb7c60f89e83f8ab1f93c133488f984e',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling ANGLE
  # and whatever else without interference from each other.
  'angle_revision': '90ed1e4971ae274d125654a0508d60a75a4be252',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling build tools
  # and whatever else without interference from each other.
  'buildtools_revision': '461b345a815c1c745ac0534a6a4bd52d123abe68',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling SwiftShader
  # and whatever else without interference from each other.
  'swiftshader_revision': '3e2b10936c5304477fdadfa233671738008fe154',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling PDFium
  # and whatever else without interference from each other.
  'pdfium_revision': '120c19b6a19377c341658c342c4351eccbb59741',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling openmax_dl
  # and whatever else without interference from each other.
  'openmax_dl_revision': '7acede9c039ea5d14cf326f44aad1245b9e674a7',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling BoringSSL
  # and whatever else without interference from each other.
  'boringssl_revision': '8793942c5c7f4a46dcadea3a686c50f2b1c312ed',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling google-toolbox-for-mac
  # and whatever else without interference from each other.
  'google_toolbox_for_mac_revision': '3c3111d3aefe907c8c0f0e933029608d96ceefeb',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling lighttpd
  # and whatever else without interference from each other.
  'lighttpd_revision': '9dfa55d15937a688a92cbf2b7a8621b0927d06eb',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling lss
  # and whatever else without interference from each other.
  'lss_revision': 'e6527b0cd469e3ff5764785dadcb39bf7d787154',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling NaCl
  # and whatever else without interference from each other.
  'nacl_revision': '7f07816e463da403255f0ab4f6d88004450dd09d',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling freetype
  # and whatever else without interference from each other.
  'freetype_revision': '954710ddd756733b851c0d2e472b93cbb9a6a44a',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling catapult
  # and whatever else without interference from each other.
  'catapult_revision': '37921f135dc61021b8ced4c73375339658ac9953',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libFuzzer
  # and whatever else without interference from each other.
  'libfuzzer_revision': 'a00e8070bea627f21df4a7eb1d58083f1bcfbba1',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling devtools-node-modules
  # and whatever else without interference from each other.
  'devtools_node_modules_revision': '6226d6cd80aaf2e5295ed460cf73ef6a582e4d78',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libprotobuf-mutator
  # and whatever else without interference from each other.
  'libprotobuf-mutator': '3fc43a01d721ef1bacfefed170bc22abf1b8b051',
}

# Only these hosts are allowed for dependencies in this DEPS file.
# If you need to add a new host, contact chrome infrastracture team.
allowed_hosts = [
  'android.googlesource.com',
  'aomedia.googlesource.com',
  'boringssl.googlesource.com',
  'chrome-internal.googlesource.com',
  'chromium.googlesource.com',
  'pdfium.googlesource.com',
  'skia.googlesource.com',
  'swiftshader.googlesource.com',
  'webrtc.googlesource.com',
]

deps = {
  'src/buildtools':
    Var('chromium_git') + '/chromium/buildtools.git' + '@' +  Var('buildtools_revision'),

  'src/chrome/installer/mac/third_party/xz/xz': {
      'url': Var('chromium_git') + '/chromium/deps/xz.git' + '@' + 'eecaf55632ca72e90eb2641376bce7cdbc7284f7',
      'condition': 'checkout_mac',
  },

  'src/chrome/test/data/perf/canvas_bench':
    Var('chromium_git') + '/chromium/canvas_bench.git' + '@' + 'a7b40ea5ae0239517d78845a5fc9b12976bfc732',

  'src/chrome/test/data/perf/frame_rate/content':
    Var('chromium_git') + '/chromium/frame_rate/content.git' + '@' + 'c10272c88463efeef6bb19c9ec07c42bc8fe22b9',

  'src/chrome/test/data/vr/webvr_info':
    Var('chromium_git') + '/external/github.com/toji/webvr.info.git' + '@' + 'c58ae99b9ff9e2aa4c524633519570bf33536248',

  'src/ios/third_party/earl_grey/src': {
      'url': Var('chromium_git') + '/external/github.com/google/EarlGrey.git' + '@' + '2fd8a7d4b76f820fb95bce495c0ceb324dbe3edb',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/fishhook/src': {
      'url': Var('chromium_git') + '/external/github.com/facebook/fishhook.git' + '@' + 'd172d5247aa590c25d0b1885448bae76036ea22c',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/gcdwebserver/src': {
      'url': Var('chromium_git') + '/external/github.com/swisspol/GCDWebServer.git' + '@' + '43555c66627f6ed44817855a0f6d465f559d30e0',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/material_components_ios/src': {
      'url': Var('chromium_git') + '/external/github.com/material-components/material-components-ios.git' + '@' + '857c3016cdf4f765263af84f41f027f51091bbc8',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/material_font_disk_loader_ios/src': {
      'url': Var('chromium_git') + '/external/github.com/material-foundation/material-font-disk-loader-ios.git' + '@' + '8e30188777b016182658fbaa0a4a020a48183224',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/material_internationalization_ios/src': {
      'url': Var('chromium_git') + '/external/github.com/material-foundation/material-internationalization-ios.git' + '@' + '5b0f22e4715305ddc3a8810f5939be3f07cb7e3e',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/material_roboto_font_loader_ios/src': {
      'url': Var('chromium_git') + '/external/github.com/material-foundation/material-roboto-font-loader-ios.git' + '@' + '4aa51e906e5671c71d24e991f1f10d782a58409f',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/material_sprited_animation_view_ios/src': {
      'url': Var('chromium_git') + '/external/github.com/material-foundation/material-sprited-animation-view-ios.git' + '@' + 'c6e16d06bdafd95540c62b3402d9414692fbca81',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/material_text_accessibility_ios/src': {
      'url': Var('chromium_git') + '/external/github.com/material-foundation/material-text-accessibility-ios.git' + '@' + '7340b22cc589101ba0b11516afe4f3a733041951',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/ochamcrest/src': {
      'url': Var('chromium_git') + '/external/github.com/hamcrest/OCHamcrest.git' + '@' + '92d9c14d13bb864255e65c09383564653896916b',
      'condition': 'checkout_ios',
  },

  'src/media/cdm/api':
    Var('chromium_git') + '/chromium/cdm.git' + '@' + 'ea5df8e78fbd0a4c24cc3a1f3faefefcd1b45237',

  'src/native_client': {
      'url': Var('chromium_git') + '/native_client/src/native_client.git' + '@' + Var('nacl_revision'),
      'condition': 'checkout_nacl',
  },

  'src/third_party/SPIRV-Tools/src':
    Var('chromium_git') + '/external/github.com/KhronosGroup/SPIRV-Tools.git' + '@' + '9166854ac93ef81b026e943ccd230fed6c8b8d3c',

  'src/third_party/android_protobuf/src': {
      'url': Var('chromium_git') + '/external/android_protobuf.git' + '@' + '7fca48d8ce97f7ba3ab8eea5c472f1ad3711762f',
      'condition': 'checkout_android',
  },

  'src/third_party/android_tools': {
      'url': Var('chromium_git') + '/android_tools.git' + '@' + '9914c5704717424998c69e837be3631914d787cc',
      'condition': 'checkout_android',
  },

  'src/third_party/angle':
    Var('chromium_git') + '/angle/angle.git' + '@' +  Var('angle_revision'),

  'src/third_party/apache-portable-runtime/src': {
      'url': Var('chromium_git') + '/external/apache-portable-runtime.git' + '@' + 'c76a8c4277e09a82eaa229e35246edea1ee0a6a1',
      'condition': 'checkout_android',
  },

  'src/third_party/auto/src': {
      'url': Var('chromium_git') + '/external/github.com/google/auto.git' + '@' + '71802f2ae74dae2744abd999f8434e13055c4ee3',
      'condition': 'checkout_android',
  },

  'src/third_party/bidichecker':
    Var('chromium_git') + '/external/bidichecker/lib.git' + '@' + '97f2aa645b74c28c57eca56992235c79850fa9e0',

  'src/third_party/bison': {
      'url': Var('chromium_git') + '/chromium/deps/bison.git' + '@' + '083c9a45e4affdd5464ee2b224c2df649c6e26c3',
      'condition': 'checkout_win',
  },

  'src/third_party/boringssl/src':
    Var('boringssl_git') + '/boringssl.git' + '@' +  Var('boringssl_revision'),

  'src/third_party/breakpad/breakpad':
    Var('chromium_git') + '/breakpad/breakpad.git' + '@' + '8a0edac9abfec72e0a86aebd6a0a38761c7c8962',

  'src/third_party/catapult':
    Var('chromium_git') + '/catapult.git' + '@' + Var('catapult_revision'),

  'src/third_party/ced/src':
    Var('chromium_git') + '/external/github.com/google/compact_enc_det.git' + '@' + '94c367a1fe3a13207f4b22604fcfd1d9f9ddf6d9',

  # Build tools for Chrome OS. Note: This depends on third_party/pyelftools.
  'src/third_party/chromite': {
      'url': Var('chromium_git') + '/chromiumos/chromite.git' + '@' + 'ef632e4a473bfe196707200bc315293b59a121f7',
      'condition': 'checkout_linux',
  },

  'src/third_party/cld_3/src':
    Var('chromium_git') + '/external/github.com/google/cld_3.git' + '@' + 'ae02d6b8a2af41e87c956c7c7d3f651a8b7b9e79',

  'src/third_party/colorama/src':
    Var('chromium_git') + '/external/colorama.git' + '@' + '799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',

  'src/third_party/crc32c/src':
    Var('chromium_git') + '/external/github.com/google/crc32c.git' + '@' + '0f771ed5ef83556451e1736f22b1a11054dc81c3',

  # For Linux and Chromium OS.
  'src/third_party/cros_system_api': {
      'url': Var('chromium_git') + '/chromiumos/platform/system_api.git' + '@' + '5fc40ab72e45c14f5d98b3b2498224079b574ebb',
      'condition': 'checkout_linux',
  },

  'src/third_party/custom_tabs_client/src': {
      'url': Var('chromium_git') + '/custom-tabs-client.git' + '@' + 'afe8a13457103292fb1ba49877a0b0e5abdc2f36',
      'condition': 'checkout_android',
  },

  'src/third_party/depot_tools':
    Var('chromium_git') + '/chromium/tools/depot_tools.git' + '@' + 'ddfead3483f2c1b49419de2b9f561cb32763f2b3',

  # DevTools node modules. Used on Linux buildbots only.
  'src/third_party/devtools-node-modules': {
      'url': Var('chromium_git') + '/external/github.com/ChromeDevTools/devtools-node-modules' + '@' + Var('devtools_node_modules_revision'),
      'condition': 'checkout_linux',
  },

  'src/third_party/dom_distiller_js/dist':
    Var('chromium_git') + '/chromium/dom-distiller/dist.git' + '@' + '7a530fc00184ee28e79a2b45270906cacaf00d3b',

  'src/third_party/elfutils/src': {
      'url': Var('chromium_git') + '/external/elfutils.git' + '@' + '249673729a7e5dbd5de4f3760bdcaa3d23d154d7',
      'condition': 'checkout_android',
  },

  'src/third_party/errorprone/lib': {
      'url': Var('chromium_git') + '/chromium/third_party/errorprone.git' + '@' + '635c410fc42614b48518ae6818105bc83af88497',
      'condition': 'checkout_android',
  },

  'src/third_party/ffmpeg':
    Var('chromium_git') + '/chromium/third_party/ffmpeg.git' + '@' + '168dfaa19bfdaa60e58a675eea67d2ee700fcdf9',

  'src/third_party/findbugs': {
      'url': Var('chromium_git') + '/chromium/deps/findbugs.git' + '@' + '4275d9ac8610db6b1bc9a5e887f97e41b33fac67',
      'condition': 'checkout_android',
  },

  'src/third_party/flac':
    Var('chromium_git') + '/chromium/deps/flac.git' + '@' + '7d0f5b3a173ffe98db08057d1f52b7787569e0a6',

  'src/third_party/flatbuffers/src':
    Var('chromium_git') + '/external/github.com/google/flatbuffers.git' + '@' + '01c50d57a67a52ee3cddd81b54d4647e9123a290',

  # Used for embedded builds. CrOS & Linux use the system version.
  'src/third_party/fontconfig/src': {
      'url': Var('chromium_git') + '/external/fontconfig.git' + '@' + 'f16c3118e25546c1b749f9823c51827a60aeb5c1',
      'condition': 'checkout_linux',
  },

  'src/third_party/freetype/src':
    Var('chromium_git') + '/chromium/src/third_party/freetype2.git' + '@' + Var('freetype_revision'),

  # Chrome OS touchpad gestures library.
  'src/third_party/gestures/gestures': {
      'url': Var('chromium_git') + '/chromiumos/platform/gestures.git' + '@' + '74f55100df966280d305d5d5ada824605f875839',
      'condition': 'checkout_linux',
  },

  'src/third_party/glslang/src':
    Var('chromium_git') + '/external/github.com/google/glslang.git' + '@' + '210c6bf4d8119dc5f8ac21da2d4c87184f7015e0',

  'src/third_party/google_toolbox_for_mac/src': {
      'url': Var('chromium_git') + '/external/github.com/google/google-toolbox-for-mac.git' + '@' + Var('google_toolbox_for_mac_revision'),
      'condition': 'checkout_ios or checkout_mac',
  },

  'src/third_party/googletest/src':
    Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' + '7f8fefabedf2965980585be8c2bff97458f28e0b',

  # GNU binutils assembler for x86-32.
  'src/third_party/gnu_binutils': {
      'url': Var('chromium_git') + '/native_client/deps/third_party/gnu_binutils.git' + '@' + 'f4003433b61b25666565690caf3d7a7a1a4ec436',
      'condition': 'checkout_nacl and checkout_win',
  },

  'src/third_party/gperf': {
      'url': Var('chromium_git') + '/chromium/deps/gperf.git' + '@' + 'd892d79f64f9449770443fb06da49b5a1e5d33c1',
      'condition': 'checkout_win',
  },

  'src/third_party/gvr-android-sdk/src': {
      'url': Var('chromium_git') + '/external/github.com/googlevr/gvr-android-sdk.git' + '@' + '6aef4ab34276aac81ee38f1cb1d034de7fc4a4eb',
      'condition': 'checkout_android',
  },

  'src/third_party/hunspell_dictionaries':
    Var('chromium_git') + '/chromium/deps/hunspell_dictionaries.git' + '@' + 'a9bac57ce6c9d390a52ebaad3259f5fdb871210e',

  'src/third_party/icu':
    Var('chromium_git') + '/chromium/deps/icu.git' + '@' + '5ed26985c0171755d7542e7ad6bce12cde4dccfb',

  'src/third_party/jsoncpp/source':
    Var('chromium_git') + '/external/github.com/open-source-parsers/jsoncpp.git' + '@' + 'f572e8e42e22cfcf5ab0aea26574f408943edfa4', # from svn 248

  'src/third_party/jsr-305/src': {
      'url': Var('chromium_git') + '/external/jsr-305.git' + '@' + '642c508235471f7220af6d5df2d3210e3bfc0919',
      'condition': 'checkout_android',
  },

  'src/third_party/junit/src': {
      'url': Var('chromium_git') + '/external/junit.git' + '@' + '64155f8a9babcfcf4263cf4d08253a1556e75481',
      'condition': 'checkout_android',
  },

  'src/third_party/leakcanary/src': {
      'url': Var('chromium_git') + '/external/github.com/square/leakcanary.git' + '@' + '608ded739e036a3aa69db47ac43777dcee506f8e',
      'condition': 'checkout_android',
  },

  'src/third_party/leveldatabase/src':
    Var('chromium_git') + '/external/leveldb.git' + '@' + 'ca216e493f32278f50a823811ab95f64cf0f839b',

  'src/third_party/libFuzzer/src':
    Var('chromium_git') + '/chromium/llvm-project/compiler-rt/lib/fuzzer.git' + '@' +  Var('libfuzzer_revision'),

  'src/third_party/libaddressinput/src':
    Var('chromium_git') + '/external/libaddressinput.git' + '@' + '2a7c3d38915138189f4ac78e1188b58b2fbf5219',

  'src/third_party/libaom/source/libaom': {
    'url': Var('aomedia_git') + '/aom.git' + '@' +  '535ecf6b31fe97f704f6725989cffad88ad960d8',
    'condition': 'checkout_libaom',
  },

  # Userspace interface to kernel DRM services.
  'src/third_party/libdrm/src': {
      'url': Var('chromium_git') + '/chromiumos/third_party/libdrm.git' + '@' + '16ffb1e6fce0fbd57f7a1e76021c575a40f6dc7a',
      'condition': 'checkout_linux',
  },

  # The libevdev library (Chrome OS version).
  'src/third_party/libevdev/src': {
      'url': Var('chromium_git') + '/chromiumos/platform/libevdev.git' + '@' + '9f7a1961eb4726211e18abd147d5a11a4ea86744',
      'condition': 'checkout_linux',
  },

  'src/third_party/libjpeg_turbo':
    Var('chromium_git') + '/chromium/deps/libjpeg_turbo.git' + '@' + 'a1750dbc79a8792dde3d3f7d7d8ac28ba01ac9dd',

  'src/third_party/liblouis/src': {
      'url': Var('chromium_git') + '/external/liblouis-github.git' + '@' + '5f9c03f2a3478561deb6ae4798175094be8a26c2',
      'condition': 'checkout_linux',
  },

  'src/third_party/libphonenumber/dist':
    Var('chromium_git') + '/external/libphonenumber.git' + '@' + 'a4da30df63a097d67e3c429ead6790ad91d36cf4',

  'src/third_party/libprotobuf-mutator/src':
    Var('chromium_git') + '/external/github.com/google/libprotobuf-mutator.git' + '@' +  Var('libprotobuf-mutator'),

  'src/third_party/libsrtp':
    Var('chromium_git') + '/chromium/deps/libsrtp.git' + '@' + '1d45b8e599dc2db6ea3ae22dbc94a8c504652423',

  # Android Explicit Synchronization.
  'src/third_party/libsync/src': {
      'url': Var('chromium_git') + '/aosp/platform/system/core/libsync.git' + '@' + 'aa6cda6f638bd57d3a024f0d201f723a5c3bb875',
      'condition': 'checkout_linux',
  },

  'src/third_party/libvpx/source/libvpx':
    Var('chromium_git') + '/webm/libvpx.git' + '@' +  'ea14a1a965c51a4220427d9e8b34eec92d58a997',

  'src/third_party/libwebm/source':
    Var('chromium_git') + '/webm/libwebm.git' + '@' + '4956b2dec65352af32dc71bab553acb631c64177',

  'src/third_party/libyuv':
    Var('chromium_git') + '/libyuv/libyuv.git' + '@' + '12c904a97c81c3ef4cab0fc8fb1f0485b4ec4e8c',  # from r1678

  'src/third_party/lighttpd': {
      'url': Var('chromium_git') + '/chromium/deps/lighttpd.git' + '@' + Var('lighttpd_revision'),
      'condition': 'checkout_mac or checkout_win',
  },

  'src/third_party/lss': {
      'url': Var('chromium_git') + '/linux-syscall-support.git' + '@' + Var('lss_revision'),
      'condition': 'checkout_android or checkout_linux',
  },

  'src/third_party/material_design_icons/src': {
      'url': Var('chromium_git') + '/external/github.com/google/material-design-icons.git' + '@' +
          '5ab428852e35dc177a8c37a2df9dc9ccf768c65a',
      'condition': 'checkout_ios',
  },

  'src/third_party/mesa/src':
    Var('chromium_git') + '/chromium/deps/mesa.git' + '@' + 'ef811c6bd4de74e13e7035ca882cc77f85793fef',

  # GNU binutils assembler for x86-64.
  'src/third_party/mingw-w64/mingw/bin': {
      'url': Var('chromium_git') + '/native_client/deps/third_party/mingw-w64/mingw/bin.git' + '@' + '3cc8b140b883a9fe4986d12cfd46c16a093d3527',
      'condition': 'checkout_nacl and checkout_win',
  },

  # Graphics buffer allocator for Chrome OS.
  'src/third_party/minigbm/src': {
      'url': Var('chromium_git') + '/chromiumos/platform/minigbm.git' + '@' + '27a7e6a24709564e18c3382d0aeda0b40c7ae03b',
      'condition': 'checkout_linux',
  },

  # Minizip library. Used on Chrome OS.
  'src/third_party/minizip/src': {
      'url': Var('chromium_git') + '/external/github.com/nmoinvaz/minizip' + '@' + 'e07e141475220196b55294c8172b274cc32d642d',
      'condition': 'checkout_linux',
  },

  'src/third_party/mockito/src': {
      'url': Var('chromium_git') + '/external/mockito/mockito.git' + '@' + 'de83ad4598ad4cf5ea53c69a8a8053780b04b850',
      'condition': 'checkout_android',
  },

  # Binaries for nacl sdk.
  'src/third_party/nacl_sdk_binaries': {
      'url': Var('chromium_git') + '/chromium/deps/nacl_sdk_binaries.git' + '@' + '759dfca03bdc774da7ecbf974f6e2b84f43699a5',
      'condition': 'checkout_nacl and checkout_win',
  },

  'src/third_party/netty-tcnative/src': {
      'url': Var('chromium_git') + '/external/netty-tcnative.git' + '@' + '5b46a8ef4a39c39c576fcdaaf718b585d75df463',
      'condition': 'checkout_android',
  },

  'src/third_party/netty4/src': {
      'url': Var('chromium_git') + '/external/netty4.git' + '@' + 'cc4420b13bb4eeea5b1cf4f93b2755644cd3b120',
      'condition': 'checkout_android',
  },

  'src/third_party/openh264/src':
    Var('chromium_git') + '/external/github.com/cisco/openh264' + '@' + 'a180c9d4d6f1a4830ca9eed9d159d54996bd63cb',

  'src/third_party/openmax_dl':
    Var('webrtc_git') + '/deps/third_party/openmax.git' + '@' +  Var('openmax_dl_revision'),

  'src/third_party/pdfium':
    Var('pdfium_git') + '/pdfium.git' + '@' +  Var('pdfium_revision'),

  # Parses Windows PE/COFF executable format.
  'src/third_party/pefile': {
      'url': Var('chromium_git') + '/external/pefile.git' + '@' + '72c6ae42396cb913bcab63c15585dc3b5c3f92f1',
      'condition': 'checkout_win',
  },

  'src/third_party/perl': {
      'url': Var('chromium_git') + '/chromium/deps/perl.git' + '@' + 'ac0d98b5cee6c024b0cffeb4f8f45b6fc5ccdb78',
      'condition': 'checkout_win',
  },

  # Dependency of chromite.git and skia.
  'src/third_party/pyelftools': {
      'url': Var('chromium_git') + '/chromiumos/third_party/pyelftools.git' + '@' + '19b3e610c86fcadb837d252c794cb5e8008826ae',
      'condition': 'checkout_linux',
  },

  'src/third_party/pyftpdlib/src':
    Var('chromium_git') + '/external/pyftpdlib.git' + '@' + '2be6d65e31c7ee6320d059f581f05ae8d89d7e45',

  'src/third_party/pywebsocket/src':
    Var('chromium_git') + '/external/github.com/google/pywebsocket.git' + '@' + '2d7b73c3acbd0f41dcab487ae5c97c6feae06ce2',

  'src/third_party/re2/src':
    Var('chromium_git') + '/external/github.com/google/re2.git' + '@' + '4be7a14432ff0a2ed88fd884fd76a63f0a3a6127',

  'src/third_party/requests/src': {
      'url': Var('chromium_git') + '/external/github.com/kennethreitz/requests.git' + '@' + 'f172b30356d821d180fa4ecfa3e71c7274a32de4',
      'condition': 'checkout_android',
  },

  'src/third_party/robolectric/robolectric': {
      'url': Var('chromium_git') + '/external/robolectric.git' + '@' + '4a3f2156ab7eb5213dffc3afe2d08b78dedb1478',
      'condition': 'checkout_android',
  },

  'src/third_party/sfntly/src':
    Var('chromium_git') + '/external/github.com/googlei18n/sfntly.git' + '@' + Var('sfntly_revision'),

  'src/third_party/shaderc/src':
    Var('chromium_git') + '/external/github.com/google/shaderc.git' + '@' + 'cd8793c34907073025af2622c28bcee64e9879a4',

  'src/third_party/skia':
    Var('skia_git') + '/skia.git' + '@' +  Var('skia_revision'),

  'src/third_party/smhasher/src':
    Var('chromium_git') + '/external/smhasher.git' + '@' + 'e87738e57558e0ec472b2fc3a643b838e5b6e88f',

  'src/third_party/snappy/src':
    Var('chromium_git') + '/external/github.com/google/snappy.git' + '@' + 'b02bfa754ebf27921d8da3bd2517eab445b84ff9',

  'src/third_party/swiftshader':
    Var('swiftshader_git') + '/SwiftShader.git' + '@' +  Var('swiftshader_revision'),

  'src/third_party/ub-uiautomator/lib': {
      'url': Var('chromium_git') + '/chromium/third_party/ub-uiautomator.git' + '@' + '00270549ce3161ae72ceb24712618ea28b4f9434',
      'condition': 'checkout_android',
  },

  'src/third_party/usrsctp/usrsctplib':
    Var('chromium_git') + '/external/github.com/sctplab/usrsctp' + '@' + 'f4819e1b177f7bfdd761c147f5a649b9f1a78c06',

  'src/third_party/visualmetrics/src':
    Var('chromium_git') + '/external/github.com/WPO-Foundation/visualmetrics.git' + '@' +  '1edde9d2fe203229c895b648fdec355917200ad6',

  # Display server protocol for Linux.
  'src/third_party/wayland/src': {
      'url': Var('chromium_git') + '/external/anongit.freedesktop.org/git/wayland/wayland.git' + '@' + '1361da9cd5a719b32d978485a29920429a31ed25',
      'condition': 'checkout_linux',
  },

  # Wayland protocols that add functionality not available in the core protocol.
  'src/third_party/wayland-protocols/src': {
      'url': Var('chromium_git') + '/external/anongit.freedesktop.org/git/wayland/wayland-protocols.git' + '@' + '26c99346ab5f2273fe5581bc4f6397bbb834f747',
      'condition': 'checkout_linux',
  },

  # Wireless Display Software. Used on Chrome OS.
  'src/third_party/wds/src': {
      'url': Var('chromium_git') + '/external/github.com/01org/wds' + '@' + 'ac3d8210d95f3000bf5c8e16a79dbbbf22d554a5',
      'condition': 'checkout_linux',
  },

  'src/third_party/webdriver/pylib':
    Var('chromium_git') + '/external/selenium/py.git' + '@' + '5fd78261a75fe08d27ca4835fb6c5ce4b42275bd',

  'src/third_party/webgl/src':
    Var('chromium_git') + '/external/khronosgroup/webgl.git' + '@' + 'e4919fa03c74bd561dcabf3e61668fa3c7e54353',

  'src/third_party/webrtc':
    Var('webrtc_git') + '/src.git' + '@' + '2707fb2782e7a47d0f53cf8e561bef0bc00fad66', # commit position 20628

  'src/third_party/xdg-utils': {
      'url': Var('chromium_git') + '/chromium/deps/xdg-utils.git' + '@' + 'd80274d5869b17b8c9067a1022e4416ee7ed5e0d',
      'condition': 'checkout_linux',
  },

  'src/third_party/yasm/source/patched-yasm':
    Var('chromium_git') + '/chromium/deps/yasm/patched-yasm.git' + '@' + 'b98114e18d8b9b84586b10d24353ab8616d4c5fc',

  'src/tools/gyp':
    Var('chromium_git') + '/external/gyp.git' + '@' + 'd61a9397e668fa9843c4aa7da9e79460fe590bfb',

  'src/tools/page_cycler/acid3':
    Var('chromium_git') + '/chromium/deps/acid3.git' + '@' + '6be0a66a1ebd7ebc5abc1b2f405a945f6d871521',

  'src/tools/swarming_client':
    Var('chromium_git') + '/infra/luci/client-py.git' + '@' +  Var('swarming_revision'),

  'src/v8':
    Var('chromium_git') + '/v8/v8.git' + '@' +  Var('v8_revision'),

  'src-internal': {
    'url': 'https://chrome-internal.googlesource.com/chrome/src-internal.git@d79c2901f5f810e9763cd7a816c3b87482877550',
    'condition': 'checkout_src_internal',
  },
}


include_rules = [
  # Everybody can use some things.
  # NOTE: THIS HAS TO STAY IN SYNC WITH third_party/DEPS which disallows these.
  '+base',
  '+build',
  '+ipc',

  # Everybody can use headers generated by tools/generate_library_loader.
  '+library_loaders',

  '+testing',
  '+third_party/icu/source/common/unicode',
  '+third_party/icu/source/i18n/unicode',
  '+url',
]


# checkdeps.py shouldn't check include paths for files in these dirs:
skip_child_includes = [
  'native_client_sdk',
  'out',
  'skia',
  'testing',
  'third_party/breakpad/breakpad',
  'v8',
  'win8',
]


hooks = [
  {
    # This clobbers when necessary (based on get_landmines.py). It must be the
    # first hook so that other things that get/generate into the output
    # directory will not subsequently be clobbered.
    'name': 'landmines',
    'pattern': '.',
    'action': [
        'vpython',
        'src/build/landmines.py',
    ],
  },
  {
    # Ensure that the DEPS'd "depot_tools" has its self-update capability
    # disabled.
    'name': 'disable_depot_tools_selfupdate',
    'pattern': '.',
    'action': [
        'vpython',
        'src/third_party/depot_tools/update_depot_tools_toggle.py',
        '--disable',
    ],
  },
  {
    # Ensure that while generating dependencies lists in .gyp files we don't
    # accidentally reference any .pyc files whose corresponding .py files have
    # already been deleted.
    # We should actually try to avoid generating .pyc files, crbug.com/500078.
    'name': 'remove_stale_pyc_files',
    'pattern': '.',
    'action': [
        'vpython',
        'src/tools/remove_stale_pyc_files.py',
        'src/android_webview/tools',
        'src/build/android',
        'src/gpu/gles2_conform_support',
        'src/infra',
        'src/ppapi',
        'src/printing',
        'src/third_party/catapult',
        'src/third_party/closure_compiler/build',
        'src/third_party/WebKit/Tools/Scripts',  # See http://crbug.com/625877.
        'src/tools',
    ],
  },
  {
    # This downloads binaries for Native Client's newlib toolchain.
    # Done in lieu of building the toolchain from scratch as it can take
    # anywhere from 30 minutes to 4 hours depending on platform to build.
    'name': 'nacltools',
    'pattern': '.',
    'condition': 'checkout_nacl',
    'action': [
        'vpython',
        'src/build/download_nacl_toolchains.py',
        '--mode', 'nacl_core_sdk',
        'sync', '--extract',
    ],
  },
  {
    # Downloads the current stable linux sysroot to build/linux/ if needed.
    # This sysroot updates at about the same rate that the chrome build deps
    # change. This script is a no-op except for linux users who are doing
    # official chrome builds or cross compiling.
    'name': 'sysroot',
    'pattern': '.',
    'condition': 'checkout_linux',
    'action': ['vpython', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--running-as-hook'],
  },
  {
    # Update the Windows toolchain if necessary.  Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': ['vpython', 'src/build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'checkout_ios or checkout_mac',
    'action': ['python', 'src/build/mac_toolchain.py'],
  },
  # Pull binutils for linux, enabled debug fission for faster linking /
  # debugging when used with clang on Ubuntu Precise.
  # https://code.google.com/p/chromium/issues/detail?id=352046
  {
    'name': 'binutils',
    'pattern': 'src/third_party/binutils',
    'condition': 'host_os == "linux"',
    'action': [
        'python',
        'src/third_party/binutils/download.py',
    ],
  },
  {
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['vpython', 'src/tools/clang/scripts/update.py'],
  },
  {
    # Mac doesn't use lld so it's not included in the default clang bundle
    # there.  lld is however needed in win cross builds, so download it there.
    # Should run after the clang hook.
    'name': 'lld/mac',
    'pattern': '.',
    'condition': 'host_os == "mac" and checkout_win',
    'action': ['vpython', 'src/tools/clang/scripts/download_lld_mac.py'],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['vpython', 'src/build/util/lastchange.py',
               '-o', 'src/build/util/LASTCHANGE'],
  },
  {
    # Update GPU lists version string (for gpu/config).
    'name': 'gpu_lists_version',
    'pattern': '.',
    'action': ['vpython', 'src/build/util/lastchange.py',
               '-m', 'GPU_LISTS_VERSION',
               '--revision-id-only',
               '--header', 'src/gpu/config/gpu_lists_version.h'],
  },
  {
    # Update skia_commit_hash.h.
    'name': 'lastchange_skia',
    'pattern': '.',
    'action': ['vpython', 'src/build/util/lastchange.py',
               '-m', 'SKIA_COMMIT_HASH',
               '-s', 'src/third_party/skia',
               '--header', 'src/skia/ext/skia_commit_hash.h'],
  },
  # Pull GN binaries. This needs to be before running GYP below.
  {
    'name': 'gn_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'src/buildtools/win/gn.exe.sha1',
    ],
  },
  {
    'name': 'gn_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'src/buildtools/mac/gn.sha1',
    ],
  },
  {
    'name': 'gn_linux64',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'src/buildtools/linux64/gn.sha1',
    ],
  },
  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'src/buildtools/win/clang-format.exe.sha1',
    ],
  },
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'src/buildtools/mac/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'src/buildtools/linux64/clang-format.sha1',
    ],
  },
  # Pull rc binaries using checked-in hashes.
  {
    'name': 'rc_win',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "win"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'src/build/toolchain/win/rc/win/rc.exe.sha1',
    ],
  },
  {
    'name': 'rc_mac',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "mac"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'src/build/toolchain/win/rc/mac/rc.sha1',
    ],
  },
  {
    'name': 'rc_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'src/build/toolchain/win/rc/linux64/rc.sha1',
    ]
  },
  {
    'name': 'content_shell_fonts',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--extract',
                '--no_auth',
                '--bucket', 'chromium-fonts',
                '-s', 'src/third_party/content_shell_fonts/content_shell_test_fonts.tar.gz.sha1',
    ],
  },
  # Pull order files for the win/clang build.
  {
    'name': 'orderfiles_win',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/orderfiles',
                '-d', 'src/chrome/build',
    ],
  },
  # Pull luci-go binaries (isolate, swarming) using checked-in hashes.
  {
    'name': 'luci-go_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-luci',
                '-d', 'src/tools/luci-go/win64',
    ],
  },
  {
    'name': 'luci-go_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-luci',
                '-d', 'src/tools/luci-go/mac64',
    ],
  },
  {
    'name': 'luci-go_linux',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-luci',
                '-d', 'src/tools/luci-go/linux64',
    ],
  },
  # Pull the Syzygy binaries, used for optimization and instrumentation.
  {
    'name': 'syzygy-binaries',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': ['vpython',
               'src/build/get_syzygy_binaries.py',
               '--output-dir=src/third_party/syzygy/binaries',
               '--revision=190dbfe74c6f5b5913820fa66d9176877924d7c5',
               '--overwrite',
               '--copy-dia-binaries',
    ],
  },
  {
    'name': 'apache_win32',
    'pattern': '\\.sha1',
    'condition': 'host_os == "win"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--directory',
                '--recursive',
                '--no_auth',
                '--num_threads=16',
                '--bucket', 'chromium-apache-win32',
                'src/third_party/apache-win32',
    ],
  },
  {
    'name': 'msan_chained_origins',
    'pattern': '.',
    'condition': 'checkout_instrumented_libraries',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                "--no_resume",
                "--no_auth",
                "--bucket", "chromium-instrumented-libraries",
                "-s", "src/third_party/instrumented_libraries/binaries/msan-chained-origins-trusty.tgz.sha1",
              ],
  },
  {
    'name': 'msan_no_origins',
    'pattern': '.',
    'condition': 'checkout_instrumented_libraries',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                "--no_resume",
                "--no_auth",
                "--bucket", "chromium-instrumented-libraries",
                "-s", "src/third_party/instrumented_libraries/binaries/msan-no-origins-trusty.tgz.sha1",
              ],
  },
  {
    "name": "wasm_fuzzer",
    "pattern": ".",
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                "--no_resume",
                "--no_auth",
                "-u",
                "--bucket", "v8-wasm-fuzzer",
                "-s", "src/v8/test/fuzzer/wasm_corpus.tar.gz.sha1",
    ],
  },
  {
    'name': 'devtools_install_node',
    'action': [ 'vpython',
                'src/third_party/WebKit/Source/devtools/scripts/local_node/node.py',
                '--running-as-hook',
                '--version',
    ],
  },

  # Pull down Node binaries for WebUI toolchain.
  {
    'name': 'node_linux64',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--extract',
                '--no_auth',
                '--bucket', 'chromium-nodejs/6.9.4',
                '-s', 'src/third_party/node/linux/node-linux-x64.tar.gz.sha1',
    ],
  },
  {
    'name': 'node_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--extract',
                '--no_auth',
                '--bucket', 'chromium-nodejs/6.9.4',
                '-s', 'src/third_party/node/mac/node-darwin-x64.tar.gz.sha1',
    ],
  },
  {
    'name': 'node_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-nodejs/6.9.4',
                '-s', 'src/third_party/node/win/node.exe.sha1',
    ],
  },

  # Pull down NPM dependencies for WebUI toolchain.
  {
    'name': 'webui_node_modules',
    'pattern': '.',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--extract',
                '--no_auth',
                '--bucket', 'chromium-nodejs',
                '-s', 'src/third_party/node/node_modules.tar.gz.sha1',
    ],
  },

  # Download Telemetry's binary dependencies via conditionals
  {
    'name': 'checkout_telemetry_binary_dependencies',
    'condition': 'checkout_telemetry_dependencies',
    'pattern': '.',
    'action': [ 'vpython',
                'src/third_party/catapult/telemetry/bin/fetch_telemetry_binary_dependencies',
    ],
  },
	#
  # Download Telemetry's benchmark binary dependencies via conditionals
  {
    'name': 'checkout_telemetry_benchmark_deps',
    'condition': 'checkout_telemetry_dependencies',
    'pattern': '.',
    'action': [ 'vpython',
                'src/tools/perf/fetch_benchmark_deps.py',
                '-f',
    ],
  },

  # Download Telemetry's binary dependencies
  # TODO(crbug.com/780967) - remove this once the bots are setting the
  # `checkout_telemetry_dependencies` condition.
  {
    'name': 'fetch_telemetry_binary_dependencies',
    'pattern': '.',
    'action': [ 'vpython',
                'src/tools/perf/conditionally_execute',
                '--gyp-condition', 'fetch_telemetry_dependencies=1',
                'src/third_party/catapult/telemetry/bin/fetch_telemetry_binary_dependencies',
    ],
  },

  # This is used to ensure that all network operations are properly
  # annotated so we can document what they're for.
  {
    'name': 'tools_traffic_annotation_linux',
    'pattern': '.',
    'condition': 'host_os == "linux" and checkout_traffic_annotation_tools',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--num_threads=4',
                '--bucket', 'chromium-tools-traffic_annotation',
                '-d', 'src/tools/traffic_annotation/bin/linux64',
    ],
  },

  # Pull down Zucchini test data.
  {
    'name': 'zucchini_testdata',
    'pattern': '.',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--num_threads=4',
                '--bucket', 'chromium-binary-patching',
                '-d', 'src/chrome/installer/zucchini/testdata',
    ],
  },

  {
    'name': 'Android CIPD Ensure',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': ['src/build/cipd/cipd_wrapper.py',
               '--chromium-root', 'src',
               '--ensure-file', 'src/build/cipd/android/android.ensure',
    ],
  },
  {
    # This downloads SDK extras and puts them in the
    # third_party/android_tools/sdk/extras directory.
    'name': 'sdkextras',
    'pattern': '.',
    'condition': 'checkout_android',
    # When adding a new sdk extras package to download, add the package
    # directory and zip file to .gitignore in third_party/android_tools.
    'action': [ 'vpython',
                'src/build/android/play_services/update.py',
                'download'
    ],
  },
  # Download checkstyle for use in PRESUBMIT for Java changes.
  {
    'name': 'checkstyle',
    'pattern': '.',
    # Must also be downloaded on linux for use on chromium_presubmit.
    'condition': 'checkout_android or checkout_linux',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-android-tools/checkstyle',
                '-s', 'src/third_party/checkstyle/checkstyle-8.0-all.jar.sha1'
    ],
  },
  {
    'name': 'gvr_static_shim_android_arm',
    'pattern': '\\.sha1',
    'condition': 'checkout_android',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-gvr-static-shim',
                '-s', 'src/third_party/gvr-android-sdk/libgvr_shim_static_arm.a.sha1',
    ],
  },
  {
    'name': 'gvr_static_shim_android_arm64',
    'pattern': '\\.sha1',
    'condition': 'checkout_android',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-gvr-static-shim',
                '-s', 'src/third_party/gvr-android-sdk/libgvr_shim_static_arm64.a.sha1',
      ],
  },
  {
    'name': 'vr_controller_test_api',
    'pattern': '\\.sha1',
    'condition': 'checkout_android',
    'action': [ 'vpython',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-gvr-static-shim/controller_test_api',
                '-s', 'src/third_party/gvr-android-sdk/test-libraries/controller_test_api.aar.sha1',
    ],
  },
  # Download VR test APKs only if the environment variable is set
  {
    'name': 'vr_test_apks',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': [ 'vpython',
                'src/third_party/gvr-android-sdk/test-apks/update.py',
    ],
  },
  {
    # Pull doclava binaries if building for Android.
    'name': 'doclava',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': [ 'vpython',
                'src/build/android/download_doclava.py',
    ],
  },

  {
    'name': 'fuchsia_sdk',
    'pattern': '.',
    'condition': 'checkout_fuchsia',
    'action': [
      'vpython',
      'src/build/fuchsia/update_sdk.py',
      '1a7e46683da9638f5bff44930f307043927c665f',
    ],
  },

  # Download and initialize "vpython" VirtualEnv environment packages.
  {
    'name': 'vpython_common',
    'pattern': '.',
    'action': [ 'vpython',
                '-vpython-spec', 'src/.vpython',
                '-vpython-tool', 'install',
    ],
  },
]

recursedeps = [
  # buildtools provides clang_format, libc++, and libc++abi
  'src/buildtools',
  # android_tools manages the NDK.
  'src/third_party/android_tools',
  # ANGLE manages DEPS that it also owns the build files for, such as dEQP.
  ("src/third_party/angle", "DEPS.chromium"),
  # src-internal has its own DEPS file to pull additional internal repos
  'src-internal',
]
