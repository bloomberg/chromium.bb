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
  'checkout_oculus_sdk',
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
  'checkout_libaom': True,

  # By default do not check out the Oculus SDK. Only available for Googlers.
  'checkout_oculus_sdk' : False,

  # TODO(dpranke): change to != "small" once != is supported.
  'checkout_traffic_annotation_tools': 'checkout_configuration == "default"',
  'checkout_instrumented_libraries': 'checkout_linux and checkout_configuration == "default"',

  'android_git': 'https://android.googlesource.com',
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
  'sfntly_revision': '16f3404e41edc9708a51fbce481f22e46b809686',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling Skia
  # and whatever else without interference from each other.
  'skia_revision': 'da2b4f2796330f5c9cce2cbf1f5238c1a1d5ecec',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling V8
  # and whatever else without interference from each other.
  'v8_revision': '0ecfe88ab8fa29e08f7a427fdf58a6e8d35fccfc',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling swarming_client
  # and whatever else without interference from each other.
  'swarming_revision': '88229872dd17e71658fe96763feaa77915d8cbd6',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling ANGLE
  # and whatever else without interference from each other.
  'angle_revision': 'e5fe7aad845474c097c28a4df898a42b28f26729',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling build tools
  # and whatever else without interference from each other.
  'buildtools_revision': '2637e7e911524502ea862870f52c36ed2723a25f',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling SwiftShader
  # and whatever else without interference from each other.
  'swiftshader_revision': 'b1d452eaeeb8272e37b87446511f2e0e95d27e6f',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling PDFium
  # and whatever else without interference from each other.
  'pdfium_revision': '026717cb667cf0c7215cf55daf794d69752fc900',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling openmax_dl
  # and whatever else without interference from each other.
  'openmax_dl_revision': 'b611996df3b8f6b151339d22c12c21f167009cb6',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling BoringSSL
  # and whatever else without interference from each other.
  'boringssl_revision': '61dedd681501616de5928fe5c0eac6640d4de0c1',
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
  'nacl_revision': '8ad1515975c09beebd2e9ea69c1f9c5ba88a5bbf',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling freetype
  # and whatever else without interference from each other.
  'freetype_revision': '4a03f17449ae45f0dacf4de4694ccd6e5e1b24d1',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling catapult
  # and whatever else without interference from each other.
  'catapult_revision': 'd38379370433d04a7f1dd4641fdefb16c4e4d975',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libFuzzer
  # and whatever else without interference from each other.
  'libfuzzer_revision': 'ba2c1cd6f87accb32b5dbce297387c56a2e53a2f',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling devtools-node-modules
  # and whatever else without interference from each other.
  'devtools_node_modules_revision': '5f7cd2497d7a643125c3b6eb910d99ba28be6899',
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
  'src/chrome/browser/resources/media_router/extension/src':
    Var('chromium_git') + '/media_router.git' + '@' + '6fc8d5201d0998eb5575dc389409d74e67b2b981',

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
      'url': Var('chromium_git') + '/external/github.com/material-components/material-components-ios.git' + '@' + '686cc976347618f75c9de58d4c1c220eaa8b4976',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/material_font_disk_loader_ios/src': {
      'url': Var('chromium_git') + '/external/github.com/material-foundation/material-font-disk-loader-ios.git' + '@' + '8e30188777b016182658fbaa0a4a020a48183224',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/material_internationalization_ios/src': {
      'url': Var('chromium_git') + '/external/github.com/material-foundation/material-internationalization-ios.git' + '@' + '8f28a55c7f35b95a587bba01a8467ea470647873',
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
      'url': Var('chromium_git') + '/external/github.com/material-foundation/material-text-accessibility-ios.git' + '@' + '92c9e56f4e07622084b3d931247db974fec55dde',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/motion_interchange_objc/src': {
      'url': Var('chromium_git') + '/external/github.com/material-motion/motion-interchange-objc.git' + '@' + '9be1e8572f8debb8dd9033ce9bd6ae56dc7ae1ab',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/motion_animator_objc/src': {
      'url': Var('chromium_git') + '/external/github.com/material-motion/motion-animator-objc.git' + '@' + 'ff39ecc69fdee46d388cc9f882201d54c3d5039c',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/motion_transitioning_objc/src': {
      'url': Var('chromium_git') + '/external/github.com/material-motion/motion-transitioning-objc.git' + '@' + '994fd02d1de3d80ed284f0c1a4b5f459b8b051a6',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/ochamcrest/src': {
      'url': Var('chromium_git') + '/external/github.com/hamcrest/OCHamcrest.git' + '@' + '92d9c14d13bb864255e65c09383564653896916b',
      'condition': 'checkout_ios',
  },

  'src/media/cdm/api':
    Var('chromium_git') + '/chromium/cdm.git' + '@' + '3fc73b9647ada546a981b7f17bb436702f914323',

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

  'src/third_party/android_ndk': {
      'url': Var('chromium_git') + '/android_ndk.git' + '@' + 'e951c37287c7d8cd915bf8d4149fd4a06d808b55',
      'condition': 'checkout_android',
  },

  'src/third_party/android_tools': {
      'url': Var('chromium_git') + '/android_tools.git' + '@' + '9a70d48fcdd68cd0e7e968f342bd767ee6323bd1',
      'condition': 'checkout_android',
  },

  'src/third_party/angle':
    Var('chromium_git') + '/angle/angle.git' + '@' +  Var('angle_revision'),

  'src/third_party/apache-portable-runtime/src': {
      'url': Var('chromium_git') + '/external/apache-portable-runtime.git' + '@' + 'c3f11fcd86b42922834cae91103cf068246c6bb6',
      'condition': 'checkout_android',
  },

  'src/third_party/auto/src': {
      'url': Var('chromium_git') + '/external/github.com/google/auto.git' + '@' + '8a81a858ae7b78a1aef71ac3905fade0bbd64e82',
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
    Var('chromium_git') + '/breakpad/breakpad.git' + '@' + 'ac4a549e29fbc8fe58212380e41fa24f36e6c3ec',

  'src/third_party/catapult':
    Var('chromium_git') + '/catapult.git' + '@' + Var('catapult_revision'),

  'src/third_party/ced/src':
    Var('chromium_git') + '/external/github.com/google/compact_enc_det.git' + '@' + '94c367a1fe3a13207f4b22604fcfd1d9f9ddf6d9',

  # Build tools for Chrome OS. Note: This depends on third_party/pyelftools.
  'src/third_party/chromite': {
      'url': Var('chromium_git') + '/chromiumos/chromite.git' + '@' + 'cfb7171bb4b4fa3f99b65ae38ed798961fbe692a',
      'condition': 'checkout_linux',
  },

  'src/third_party/cld_3/src':
    Var('chromium_git') + '/external/github.com/google/cld_3.git' + '@' + '484afe9ba7438d078e60b3a26e7fb590213c0e17',

  'src/third_party/colorama/src':
    Var('chromium_git') + '/external/colorama.git' + '@' + '799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',

  'src/third_party/crc32c/src':
    Var('chromium_git') + '/external/github.com/google/crc32c.git' + '@' + '0f771ed5ef83556451e1736f22b1a11054dc81c3',

  # For Linux and Chromium OS.
  'src/third_party/cros_system_api': {
      'url': Var('chromium_git') + '/chromiumos/platform/system_api.git' + '@' + '5ed4b3d3c6ee676a12bf73152871fc8a4dad3129',
      'condition': 'checkout_linux',
  },

  'src/third_party/custom_tabs_client/src': {
      'url': Var('chromium_git') + '/custom-tabs-client.git' + '@' + 'a0b6c6f7db4127c191ebb9ef008609224658edb2',
      'condition': 'checkout_android',
  },

  'src/third_party/depot_tools':
    Var('chromium_git') + '/chromium/tools/depot_tools.git' + '@' + '705e51807fec5f379bc32ece0448d60b1cc8a110',

  'src/third_party/devtools-node-modules':
    Var('chromium_git') + '/external/github.com/ChromeDevTools/devtools-node-modules' + '@' + Var('devtools_node_modules_revision'),

  'src/third_party/dom_distiller_js/dist':
    Var('chromium_git') + '/chromium/dom-distiller/dist.git' + '@' + '60b46718e28f553ab57e3d2bbda5b3b41456f417',

  'src/third_party/elfutils/src': {
      'url': Var('chromium_git') + '/external/elfutils.git' + '@' + '249673729a7e5dbd5de4f3760bdcaa3d23d154d7',
      'condition': 'checkout_android',
  },

  'src/third_party/errorprone/lib': {
      'url': Var('chromium_git') + '/chromium/third_party/errorprone.git' + '@' + 'ecc57c2b00627667874744b9ad8efe10734d97a8',
      'condition': 'checkout_android',
  },

  'src/third_party/ffmpeg':
    Var('chromium_git') + '/chromium/third_party/ffmpeg.git' + '@' + 'f5964c36e1dc41b8b91380b5eff0659e3a5a1549',

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
      'url': Var('chromium_git') + '/external/fontconfig.git' + '@' + 'b546940435ebfb0df575bc7a2350d1e913919c34',
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
    Var('chromium_git') + '/external/github.com/google/glslang.git' + '@' + 'ec1476b7060306fd9109faf7a4c70a20ea3b538c',

  'src/third_party/google_toolbox_for_mac/src': {
      'url': Var('chromium_git') + '/external/github.com/google/google-toolbox-for-mac.git' + '@' + Var('google_toolbox_for_mac_revision'),
      'condition': 'checkout_ios or checkout_mac',
  },

  'src/third_party/googletest/src':
    Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' + 'ea31cb15f0c2ab9f5f5b18e82311eb522989d747',

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
    Var('chromium_git') + '/chromium/deps/icu.git' + '@' + 'd888fd2a1be890f4d35e43f68d6d79f42519a357',

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
    Var('chromium_git') + '/external/libaddressinput.git' + '@' + 'a575c63de328d0c72237c55d1a05e191c245122a',

  'src/third_party/libaom/source/libaom': {
    'url': Var('aomedia_git') + '/aom.git' + '@' +  'cc92258a08d98f469dff1be288acbc322632377b',
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
      'url': Var('chromium_git') + '/aosp/platform/system/core/libsync.git' + '@' + 'f4f4387b6bf2387efbcfd1453af4892e8982faf6',
      'condition': 'checkout_linux',
  },

  'src/third_party/libvpx/source/libvpx':
    Var('chromium_git') + '/webm/libvpx.git' + '@' +  'ac54d233b6d7ba5674996353a86bae42417022ee',

  'src/third_party/libwebm/source':
    Var('chromium_git') + '/webm/libwebm.git' + '@' + 'b03c65468b06d097f27235d93d76bfc45f490ede',

  'src/third_party/libyuv':
    Var('chromium_git') + '/libyuv/libyuv.git' + '@' + 'b792e0dbc152dbeea6f4a5ba59dccd05c32eb3c2',  # from r1697

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
      'url': Var('chromium_git') + '/external/github.com/nmoinvaz/minizip' + '@' + '53a657318af1fccc4bac7ed230729302b2391d1d',
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
    Var('chromium_git') + '/external/github.com/cisco/openh264' + '@' + '2e96d62426547ac4fb5cbcd122e5f6eb68d66ee6',

  'src/third_party/openmax_dl':
    Var('webrtc_git') + '/deps/third_party/openmax.git' + '@' +  Var('openmax_dl_revision'),

  'src/third_party/pdfium':
    Var('pdfium_git') + '/pdfium.git' + '@' +  Var('pdfium_revision'),

  # Parses Windows PE/COFF executable format.
  'src/third_party/pefile': {
      'url': Var('chromium_git') + '/external/pefile.git' + '@' + '72c6ae42396cb913bcab63c15585dc3b5c3f92f1',
      'condition': 'checkout_win',
  },

  'src/third_party/perfetto':
    Var('android_git') + '/platform/external/perfetto.git' + '@' +  '135841c8077f13f14c6b80e32d391da84d2ee131',

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
    Var('chromium_git') + '/external/github.com/google/re2.git' + '@' + '5185d85264d23cfae4b38e2703703e9a4c8e974c',

  'src/third_party/requests/src': {
      'url': Var('chromium_git') + '/external/github.com/kennethreitz/requests.git' + '@' + 'f172b30356d821d180fa4ecfa3e71c7274a32de4',
      'condition': 'checkout_android',
  },

  'src/third_party/robolectric/robolectric': {
      'url': Var('chromium_git') + '/external/robolectric.git' + '@' + '7e067f1112e1502caa742f7be72d37b5678d3403',
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
    Var('chromium_git') + '/external/github.com/sctplab/usrsctp' + '@' + '0e076261b832121cf120ddc04aaff87ac3a34d30',

  'src/third_party/visualmetrics/src':
    Var('chromium_git') + '/external/github.com/WPO-Foundation/visualmetrics.git' + '@' +  '1edde9d2fe203229c895b648fdec355917200ad6',

  # Display server protocol for Linux.
  'src/third_party/wayland/src': {
      'url': Var('chromium_git') + '/external/anongit.freedesktop.org/git/wayland/wayland.git' + '@' + '1361da9cd5a719b32d978485a29920429a31ed25',
      'condition': 'checkout_linux',
  },

  # Wayland protocols that add functionality not available in the core protocol.
  'src/third_party/wayland-protocols/src': {
      'url': Var('chromium_git') + '/external/anongit.freedesktop.org/git/wayland/wayland-protocols.git' + '@' + '4f789286e4ab7f6fecc2ccb895d79362a9b2382a',
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
    Var('chromium_git') + '/external/khronosgroup/webgl.git' + '@' + 'a182a9ad3078aca566d8355eabf2d9f56f70ee82',

  'src/third_party/webrtc':
    Var('webrtc_git') + '/src.git' + '@' + 'cb768a88319bc0033af6d87cf4be03c85448796d', # commit position 21742

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
    'url': 'https://chrome-internal.googlesource.com/chrome/src-internal.git@a0d25d76f11657d40ba2657ecb9cc8ab23a43b2e',
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
        'python',
        'src/build/landmines.py',
    ],
  },
  {
    # Ensure that the DEPS'd "depot_tools" has its self-update capability
    # disabled.
    'name': 'disable_depot_tools_selfupdate',
    'pattern': '.',
    'action': [
        'python',
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
        'python',
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
        'python',
        'src/build/download_nacl_toolchains.py',
        '--mode', 'nacl_core_sdk',
        'sync', '--extract',
    ],
  },
  {
    'name': 'sysroot_arm',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm',
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm'],
  },
  {
    'name': 'sysroot_arm64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm64',
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm64'],
  },
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'checkout_linux and (checkout_x86 or checkout_x64)',
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x86'],
  },
  {
    'name': 'sysroot_mips',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_mips',
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=mips'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_x64',
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x64'],
  },
  {
    # Case-insensitivity for the Win SDK. Must run before win_toolchain below.
    'name': 'ciopfs_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/ciopfs',
                '-s', 'src/build/ciopfs.sha1',
    ]
  },
  {
    # Update the Windows toolchain if necessary.  Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': ['python', 'src/build/vs_toolchain.py', 'update', '--force'],
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
    'action': ['python', 'src/tools/clang/scripts/update.py'],
  },
  {
    # Mac doesn't use lld so it's not included in the default clang bundle
    # there.  lld is however needed in win cross builds, so download it there.
    # Should run after the clang hook.
    'name': 'lld/mac',
    'pattern': '.',
    'condition': 'host_os == "mac" and checkout_win',
    'action': ['python', 'src/tools/clang/scripts/download_lld_mac.py'],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python', 'src/build/util/lastchange.py',
               '-o', 'src/build/util/LASTCHANGE'],
  },
  {
    # Update GPU lists version string (for gpu/config).
    'name': 'gpu_lists_version',
    'pattern': '.',
    'action': ['python', 'src/build/util/lastchange.py',
               '-m', 'GPU_LISTS_VERSION',
               '--revision-id-only',
               '--header', 'src/gpu/config/gpu_lists_version.h'],
  },
  {
    # Update skia_commit_hash.h.
    'name': 'lastchange_skia',
    'pattern': '.',
    'action': ['python', 'src/build/util/lastchange.py',
               '-m', 'SKIA_COMMIT_HASH',
               '-s', 'src/third_party/skia',
               '--header', 'src/skia/ext/skia_commit_hash.h'],
  },
  # Pull GN binaries. This needs to be before running GYP below.
  {
    'name': 'gn_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [ 'python',
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
    'action': [ 'python',
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
    'action': [ 'python',
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
    'action': [ 'python',
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
    'action': [ 'python',
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
    'action': [ 'python',
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
    'action': [ 'python',
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
    'action': [ 'python',
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
    'action': [ 'python',
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
    'condition': 'checkout_linux',
    'action': [ 'download_from_google_storage',
                '--no_resume',
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
    'action': [ 'python',
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
    'action': [ 'python',
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
    'action': [ 'python',
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
    'action': [ 'python',
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
    'action': ['python',
               'src/build/get_syzygy_binaries.py',
               '--output-dir=src/third_party/syzygy/binaries',
               '--revision=8164b24ebde9c5649c9a09e88a7fc0b0fcbd1bc5',
               '--overwrite',
               '--copy-dia-binaries',
    ],
  },
  {
    'name': 'apache_win32',
    'pattern': '\\.sha1',
    'condition': 'host_os == "win"',
    'action': [ 'python',
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
    'action': [ 'python',
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
    'action': [ 'python',
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
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                "--no_resume",
                "--no_auth",
                "-u",
                "--bucket", "v8-wasm-fuzzer",
                "-s", "src/v8/test/fuzzer/wasm_corpus.tar.gz.sha1",
    ],
  },

  # Pull down Node binaries for WebUI toolchain.
  {
    'name': 'node_linux64',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--extract',
                '--no_auth',
                '--bucket', 'chromium-nodejs/8.9.1',
                '-s', 'src/third_party/node/linux/node-linux-x64.tar.gz.sha1',
    ],
  },
  {
    'name': 'node_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--extract',
                '--no_auth',
                '--bucket', 'chromium-nodejs/8.9.1',
                '-s', 'src/third_party/node/mac/node-darwin-x64.tar.gz.sha1',
    ],
  },
  {
    'name': 'node_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-nodejs/8.9.1',
                '-s', 'src/third_party/node/win/node.exe.sha1',
    ],
  },

  # Pull down NPM dependencies for WebUI toolchain.
  {
    'name': 'webui_node_modules',
    'pattern': '.',
    'action': [ 'python',
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
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--num_threads=4',
                '--bucket', 'chromium-tools-traffic_annotation',
                '-d', 'src/tools/traffic_annotation/bin/linux64',
    ],
  },

# This is used to ensure that all network operations are properly
# annotated so we can document what they're for.
  {
    'name': 'tools_traffic_annotation_windows',
    'pattern': '.',
    'condition': 'host_os == "win" and checkout_traffic_annotation_tools',
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--num_threads=4',
                '--bucket', 'chromium-tools-traffic_annotation',
                '-d', 'src/tools/traffic_annotation/bin/win32',
    ],
  },

  # Pull down Zucchini test data.
  {
    'name': 'zucchini_testdata',
    'pattern': '.',
    'action': [ 'python',
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
    'name': 'Fetch Android AFDO profile',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': ['vpython', 'src/chrome/android/profiles/update_afdo_profile.py'],
  },
  {
    # This downloads SDK extras and puts them in the
    # third_party/android_tools/sdk/extras directory.
    'name': 'sdkextras',
    'pattern': '.',
    'condition': 'checkout_android',
    # When adding a new sdk extras package to download, add the package
    # directory and zip file to .gitignore in third_party/android_tools.
    'action': [ 'python',
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
    'action': [ 'python',
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
    'action': [ 'python',
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
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-gvr-static-shim',
                '-s', 'src/third_party/gvr-android-sdk/libgvr_shim_static_arm64.a.sha1',
      ],
  },
  {
    'name': 'vr_assets',
    'pattern': '.',
    'condition': 'checkout_src_internal and checkout_android',
    'action': ['python',
               'src/third_party/depot_tools/download_from_google_storage.py',
               '--bucket', 'chrome-vr-assets',
               '--recursive',
               '--directory',
               'src/chrome/browser/resources/vr/assets',
    ],
  },
  {
    'name': 'vr_controller_test_api',
    'pattern': '\\.sha1',
    'condition': 'checkout_android',
    'action': [ 'python',
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
    'action': [ 'python',
                'src/third_party/gvr-android-sdk/test-apks/update.py',
    ],
  },
  # Download Oculus SDK if appropriate.
  {
    'name': 'libovr',
    'pattern': '.',
    'condition': 'checkout_oculus_sdk',
    'action': ['python',
               'src/third_party/depot_tools/download_from_google_storage.py',
               '--bucket', 'chrome-oculus-sdk',
               '--recursive',
               '--num_threads=10',
               '--directory',
               'src/third_party/libovr/src',
    ],
  },
  {
    # Pull doclava binaries if building for Android.
    'name': 'doclava',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': [ 'python',
                'src/build/android/download_doclava.py',
    ],
  },

  {
    'name': 'fuchsia_sdk',
    'pattern': '.',
    'condition': 'checkout_fuchsia',
    'action': [
      'python',
      'src/build/fuchsia/update_sdk.py',
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
