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
#  # Run roll-dep (provided by depot_tools) giving the dep's path and the
#  # desired SVN revision number (e.g., third_party/foo/bar and a revision such
#  # number from Subversion)
#  roll-dep third_party/foo/bar REVISION_NUMBER
#  # You should now have a modified DEPS file; commit and upload as normal
#  git commit -a
#  git cl upload


vars = {
  # Use this googlecode_url variable only if there is an internal mirror for it.
  # If you do not know, use the full path while defining your new deps entry.
  'googlecode_url': 'http://%s.googlecode.com/svn',
  'sourceforge_url': 'http://svn.code.sf.net/p/%(repo)s/code',
  'llvm_url': 'http://src.chromium.org/llvm-project',
  'llvm_git': 'https://llvm.googlesource.com',
  'webkit_trunk': 'http://src.chromium.org/blink/trunk',
  'webkit_revision': '732bda7e3e8da1b3ee8da7a237a3a9c4dc4b2e7d', # from svn revision 192165
  'chromium_git': 'https://chromium.googlesource.com',
  'chromiumos_git': 'https://chromium.googlesource.com/chromiumos',
  'pdfium_git': 'https://pdfium.googlesource.com',
  'skia_git': 'https://skia.googlesource.com',
  'boringssl_git': 'https://boringssl.googlesource.com',
  'libvpx_revision': 'f80cf581e6e56ec3b28b118aebf98499b6bc156d',
  'sfntly_revision': '1bdaae8fc788a5ac8936d68bf24f37d977a13dac',
  'skia_revision': '1a5041e446bda33e7f5e18f0df8d0be77c0a2349',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling Skia
  # and V8 without interference from each other.
  'v8_branch': 'trunk',
  'v8_revision': 'a2bf336a13d103602e8621d0dd0a337a45e71853',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling WebRTC
  # and V8 without interference from each other.
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling swarming_client
  # and whatever else without interference from each other.
  'swarming_revision': 'b61a1802f5ef4bb8c7b81060cc80add47e6cf302',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling ANGLE
  # and whatever else without interference from each other.
  'angle_revision': 'a17e77febd24b92bf0a650eb401dbe215e0d6606',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling build tools
  # and whatever else without interference from each other.
  'buildtools_revision': '3b302fef93f7cc58d9b8168466905237484b2772',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling PDFium
  # and whatever else without interference from each other.
  'pdfium_revision': '28ddd48bfd84c55cc51d0b16fa533c51affdeb5b',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling openmax_dl
  # and whatever else without interference from each other.
  'openmax_dl_revision': '0b238cb62c32b6f45680cf577eddb1b051ae0219',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling BoringSSL
  # and whatever else without interference from each other.
  'boringssl_revision': 'bf0df92964565f819881e78ff3bfb5343e95e130',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling nss
  # and whatever else without interference from each other.
  'nss_revision': 'bb4e75a43d007518ae7d618665ea2f25b0c60b63',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling google-toolbox-for-mac
  # and whatever else without interference from each other.
  'google_toolbox_for_mac_revision': '17eee6933bb4a978bf045ef1b12fc68f15b08cd2', # from svn revision 703
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling lighttpd
  # and whatever else without interference from each other.
  'lighttpd_revision': '9dfa55d15937a688a92cbf2b7a8621b0927d06eb',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling lss
  # and whatever else without interference from each other.
  'lss_revision': 'e079768b7e3a94dcbe7d338496c0c3bde7151b6e',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling NaCl
  # and whatever else without interference from each other.
  'nacl_revision': 'f418b37b22393d82c08d28ce1c151bed933d2c9b',
}

# Only these hosts are allowed for dependencies in this DEPS file.
# If you need to add a new host, contact chrome infrastracture team.
allowed_hosts = [
  'chromium.googlesource.com',
  'boringssl.googlesource.com',
  'pdfium.googlesource.com',
]

deps = {
  'src/breakpad/src':
   Var('chromium_git') + '/external/google-breakpad/src.git' + '@' + '2c37c225e729d6ada72d5ffd808406db9c916985', # from svn revision 1434

  'src/buildtools':
   Var('chromium_git') + '/chromium/buildtools.git' + '@' +  Var('buildtools_revision'),

  'src/sdch/open-vcdiff':
   Var('chromium_git') + '/external/open-vcdiff.git' + '@' + '438f2a5be6d809bc21611a94cd37bfc8c28ceb33', # from svn revision 41

  'src/testing/gtest':
   Var('chromium_git') + '/external/googletest.git' + '@' + 'be1868139ffe0ccd0e8e3b37292b84c821d9c8ad', # from svn revision 704

  'src/testing/gmock':
   Var('chromium_git') + '/external/googlemock.git' + '@' + '29763965ab52f24565299976b936d1265cb6a271', # from svn revision 501

  'src/third_party/angle':
   Var('chromium_git') + '/angle/angle.git' + '@' +  Var('angle_revision'),

  'src/third_party/colorama/src':
   Var('chromium_git') + '/external/colorama.git' + '@' + '799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',

  'src/third_party/crashpad/crashpad':
   Var('chromium_git') + '/crashpad/crashpad.git' + '@' + '82bebb11de699550a36bf411737d01880e4a16a7',

  'src/third_party/trace-viewer':
   Var('chromium_git') + '/external/trace-viewer.git' + '@' + '038d6c7c39e1a0486e0874792bec2d25159213a3',

  'src/third_party/WebKit':
   Var('chromium_git') + '/chromium/blink.git' + '@' +  Var('webkit_revision'),

  'src/third_party/icu':
   Var('chromium_git') + '/chromium/deps/icu.git' + '@' + 'd319ad9e25e8045f051dcd30bd5835f99936af6c',

  'src/third_party/libexif/sources':
   Var('chromium_git') + '/chromium/deps/libexif/sources.git' + '@' + 'ed98343daabd7b4497f97fda972e132e6877c48a',

  'src/third_party/hunspell':
   Var('chromium_git') + '/chromium/deps/hunspell.git' + '@' + 'c956c0e97af00ef789afb2f64d02c9a5a50e6eb1',

  'src/third_party/hunspell_dictionaries':
   Var('chromium_git') + '/chromium/deps/hunspell_dictionaries.git' + '@' + '80796932b89ab36431399d76c5b8391ea471e30a', # from svn revision 294404

  'src/third_party/safe_browsing/testing':
    Var('chromium_git') + '/external/google-safe-browsing/testing.git' + '@' + '9d7e8064f3ca2e45891470c9b5b1dce54af6a9d6',

  'src/third_party/cacheinvalidation/src':
    Var('chromium_git') + '/external/google-cache-invalidation-api/src.git' + '@' + 'c91bd9d9fed06bf440be64f87b94a2effdb32bc4', # from svn revision 341

  'src/third_party/leveldatabase/src':
    Var('chromium_git') + '/external/leveldb.git' + '@' + '251ebf5dc70129ad3c38193fe6c99a5b0ec6b9fa',

  'src/third_party/snappy/src':
    Var('chromium_git') + '/external/snappy.git' + '@' + '762bb32f0c9d2f31ba4958c7c0933d22e80c20bf',

  'src/tools/grit':
    Var('chromium_git') + '/external/grit-i18n.git' + '@' + '0287c187b11ed53590254e4d817e836a44a7a1a7', # from svn revision 186

  'src/tools/gyp':
    Var('chromium_git') + '/external/gyp.git' + '@' + 'd174d75bf69c682cb62af9187879e01513b35e52',

  'src/tools/swarming_client':
   Var('chromium_git') + '/external/swarming.client.git' + '@' +  Var('swarming_revision'),

  'src/v8':
    Var('chromium_git') + '/v8/v8.git' + '@' +  Var('v8_revision'),

  'src/native_client':
   Var('chromium_git') + '/native_client/src/native_client.git' + '@' + Var('nacl_revision'),

  'src/third_party/sfntly/cpp/src':
    Var('chromium_git') + '/external/sfntly/cpp/src.git' + '@' +  Var('sfntly_revision'),

  'src/third_party/skia':
   Var('chromium_git') + '/skia.git' + '@' +  Var('skia_revision'),

  'src/tools/page_cycler/acid3':
   Var('chromium_git') + '/chromium/deps/acid3.git' + '@' + '6be0a66a1ebd7ebc5abc1b2f405a945f6d871521',

  'src/chrome/test/data/perf/canvas_bench':
   Var('chromium_git') + '/chromium/canvas_bench.git' + '@' + 'a7b40ea5ae0239517d78845a5fc9b12976bfc732',

  'src/chrome/test/data/perf/frame_rate/content':
   Var('chromium_git') + '/chromium/frame_rate/content.git' + '@' + 'c10272c88463efeef6bb19c9ec07c42bc8fe22b9',

  'src/third_party/bidichecker':
    Var('chromium_git') + '/external/bidichecker/lib.git' + '@' + '97f2aa645b74c28c57eca56992235c79850fa9e0',

  'src/third_party/webgl/src':
   Var('chromium_git') + '/external/khronosgroup/webgl.git' + '@' + '0c2bcf36a740181f50ce94a0eaad357219441dee',

  'src/third_party/swig/Lib':
   Var('chromium_git') + '/chromium/deps/swig/Lib.git' + '@' + 'f2a695d52e61e6a8d967731434f165ed400f0d69',

  'src/third_party/webdriver/pylib':
    Var('chromium_git') + '/external/selenium/py.git' + '@' + '5fd78261a75fe08d27ca4835fb6c5ce4b42275bd',

  'src/third_party/libvpx':
   Var('chromium_git') + '/chromium/deps/libvpx.git' + '@' +  Var('libvpx_revision'),

  'src/third_party/ffmpeg':
   Var('chromium_git') + '/chromium/third_party/ffmpeg.git' + '@' + 'b3dc5aa44575049edf6b24e34578487dfaade6a4',

  'src/third_party/libjingle/source/talk':
    Var('chromium_git') + '/external/webrtc/trunk/talk.git' + '@' + '628865b407a88d847027364e450027c65c9a215f',

  'src/third_party/usrsctp/usrsctplib':
    Var('chromium_git') + '/external/usrsctplib.git' + '@' + '13718c7b9fd376fde092cbd3c5347d15059ac652', # from svn revision 9167

  'src/third_party/libsrtp':
   Var('chromium_git') + '/chromium/deps/libsrtp.git' + '@' + '6446144c7f083552f21cc4e6768e891bcb767574',

  'src/third_party/yasm/source/patched-yasm':
   Var('chromium_git') + '/chromium/deps/yasm/patched-yasm.git' + '@' + '4671120cd8558ce62ee8672ebf3eb6f5216f909b',

  'src/third_party/libjpeg_turbo':
   Var('chromium_git') + '/chromium/deps/libjpeg_turbo.git' + '@' + '034e9a9747e0983bc19808ea70e469bc8342081f',

  'src/third_party/flac':
   Var('chromium_git') + '/chromium/deps/flac.git' + '@' + '0635a091379d9677f1ddde5f2eec85d0f096f219',

  'src/third_party/pyftpdlib/src':
    Var('chromium_git') + '/external/pyftpdlib.git' + '@' + '2be6d65e31c7ee6320d059f581f05ae8d89d7e45',

  'src/third_party/scons-2.0.1':
   Var('chromium_git') + '/native_client/src/third_party/scons-2.0.1.git' + '@' + '1c1550e17fc26355d08627fbdec13d8291227067',

  'src/third_party/webrtc':
    Var('chromium_git') + '/external/webrtc/trunk/webrtc.git' + '@' + '78aaaa62d5d796ece87740ea41863a5d09611341',

  'src/third_party/openmax_dl':
    Var('chromium_git') + '/external/webrtc/deps/third_party/openmax.git' + '@' +  Var('openmax_dl_revision'),

  'src/third_party/jsoncpp/source/include':
    Var('chromium_git') + '/external/jsoncpp/jsoncpp/include.git' + '@' + 'b0dd48e02b6e6248328db78a65b5c601f150c349',

  'src/third_party/jsoncpp/source/src/lib_json':
    Var('chromium_git') + '/external/jsoncpp/jsoncpp/src/lib_json.git' + '@' + 'a8caa51ba2f80971a45880425bf2ae864a786784',

  'src/third_party/libyuv':
    Var('chromium_git') + '/external/libyuv.git' + '@' + 'd204db647e591ccf0e2589236ecea90330d65a66', # from svn revision 1171

  'src/third_party/smhasher/src':
    Var('chromium_git') + '/external/smhasher.git' + '@' + 'e87738e57558e0ec472b2fc3a643b838e5b6e88f',

  'src/third_party/libaddressinput/src':
    Var('chromium_git') + '/external/libaddressinput.git' + '@' + '61f63da7ae6fa469138d60dec5d6bbecc6ab43d6',

  # These are all at libphonenumber r728.
  'src/third_party/libphonenumber/src/phonenumbers':
    Var('chromium_git') + '/external/libphonenumber/cpp/src/phonenumbers.git' + '@' + '0d6e3e50e17c94262ad1ca3b7d52b11223084bca',
  'src/third_party/libphonenumber/src/test':
    Var('chromium_git') + '/external/libphonenumber/cpp/test.git' + '@' + 'f351a7e007f9c9995494499120bbc361ca808a16',
  'src/third_party/libphonenumber/src/resources':
    Var('chromium_git') + '/external/libphonenumber/resources.git' + '@' + 'b6dfdc7952571ff7ee72643cd88c988cbe966396',

  'src/tools/deps2git':
   Var('chromium_git') + '/chromium/tools/deps2git.git' + '@' + 'f04828eb0b5acd3e7ad983c024870f17f17b06d9',

  'src/third_party/webpagereplay':
   Var('chromium_git') + '/external/web-page-replay.git' + '@' + '532b413ff95e8595d5028e0dae75dcf3ba712d2e',

  'src/third_party/pywebsocket/src':
    Var('chromium_git') + '/external/pywebsocket/src.git' + '@' + 'cb349e87ddb30ff8d1fa1a89be39cec901f4a29c',

  'src/third_party/opus/src':
   Var('chromium_git') + '/chromium/deps/opus.git' + '@' + 'cae696156f1e60006e39821e79a1811ae1933c69',

  'src/media/cdm/ppapi/api':
   Var('chromium_git') + '/chromium/cdm.git' + '@' + '7b7c6cc620e13c8057b4b6bff19e5955feb2c8fa', # from svn revision 293617

  'src/third_party/mesa/src':
   Var('chromium_git') + '/chromium/deps/mesa.git' + '@' + '071d25db04c23821a12a8b260ab9d96a097402f0',

  'src/third_party/cld_2/src':
    Var('chromium_git') + '/external/cld2.git' + '@' + '14d9ef8d4766326f8aa7de54402d1b9c782d4481', # from svn revision 193

  'src/third_party/pdfium':
   'https://pdfium.googlesource.com/pdfium.git' + '@' +  Var('pdfium_revision'),

  'src/third_party/boringssl/src':
   'https://boringssl.googlesource.com/boringssl.git' + '@' +  Var('boringssl_revision'),

  'src/third_party/py_trace_event/src':
    Var('chromium_git') + '/external/py_trace_event.git' + '@' + 'dd463ea9e2c430de2b9e53dea57a77b4c3ac9b30',
}


deps_os = {
  'win': {
    'src/chrome/tools/test/reference_build/chrome_win':
     Var('chromium_git') + '/chromium/reference_builds/chrome_win.git' + '@' + 'f8a3a845dfc845df6b14280f04f86a61959357ef',

    'src/third_party/cygwin':
     Var('chromium_git') + '/chromium/deps/cygwin.git' + '@' + 'c89e446b273697fadf3a10ff1007a97c0b7de6df',

    'src/third_party/psyco_win32':
     Var('chromium_git') + '/chromium/deps/psyco_win32.git' + '@' + 'f5af9f6910ee5a8075bbaeed0591469f1661d868',

    'src/third_party/bison':
     Var('chromium_git') + '/chromium/deps/bison.git' + '@' + '083c9a45e4affdd5464ee2b224c2df649c6e26c3',

    'src/third_party/gperf':
     Var('chromium_git') + '/chromium/deps/gperf.git' + '@' + 'd892d79f64f9449770443fb06da49b5a1e5d33c1',

    'src/third_party/perl':
     Var('chromium_git') + '/chromium/deps/perl.git' + '@' + 'ac0d98b5cee6c024b0cffeb4f8f45b6fc5ccdb78',

    'src/third_party/lighttpd':
     Var('chromium_git') + '/chromium/deps/lighttpd.git' + '@' + Var('lighttpd_revision'),

    # Parses Windows PE/COFF executable format.
    'src/third_party/pefile':
     Var('chromium_git') + '/external/pefile.git' + '@' + '72c6ae42396cb913bcab63c15585dc3b5c3f92f1',

    # NSS, for SSLClientSocketNSS.
    'src/third_party/nss':
     Var('chromium_git') + '/chromium/deps/nss.git' + '@' + Var('nss_revision'),

    'src/third_party/swig/win':
     Var('chromium_git') + '/chromium/deps/swig/win.git' + '@' + '986f013ba518541adf5c839811efb35630a31031',

    # GNU binutils assembler for x86-32.
    'src/third_party/gnu_binutils':
      Var('chromium_git') + '/native_client/deps/third_party/gnu_binutils.git' + '@' + 'f4003433b61b25666565690caf3d7a7a1a4ec436',
    # GNU binutils assembler for x86-64.
    'src/third_party/mingw-w64/mingw/bin':
      Var('chromium_git') + '/native_client/deps/third_party/mingw-w64/mingw/bin.git' + '@' + '3cc8b140b883a9fe4986d12cfd46c16a093d3527',

    # Dependencies used by libjpeg-turbo
    'src/third_party/yasm/binaries':
     Var('chromium_git') + '/chromium/deps/yasm/binaries.git' + '@' + '52f9b3f4b0aa06da24ef8b123058bb61ee468881',

    # Binaries for nacl sdk.
    'src/third_party/nacl_sdk_binaries':
     Var('chromium_git') + '/chromium/deps/nacl_sdk_binaries.git' + '@' + '759dfca03bdc774da7ecbf974f6e2b84f43699a5',

    # Omaha code for src/app_installer/.
    'src/third_party/omaha/src/omaha':
     Var('chromium_git') + '/external/omaha.git' + '@' + '098c7a3d157218dab4eed595e8f2fbe5a20a0bae',
  },
  'ios': {
    'src/ios/third_party/gcdwebserver/src':
     Var('chromium_git') + '/external/github.com/swisspol/GCDWebServer.git' + '@' + '18889793b75d7ee593d62ac88997caad850acdb6',

    'src/third_party/google_toolbox_for_mac/src':
      Var('chromium_git') + '/external/google-toolbox-for-mac.git' + '@' + Var('google_toolbox_for_mac_revision'),

    'src/third_party/nss':
     Var('chromium_git') + '/chromium/deps/nss.git' + '@' + Var('nss_revision'),

    # class-dump utility to generate header files for undocumented SDKs
    'src/testing/iossim/third_party/class-dump':
     Var('chromium_git') + '/chromium/deps/class-dump.git' + '@' + '89bd40883c767584240b4dade8b74e6f57b9bdab',

    # Code that's not needed due to not building everything
    'src/chrome/test/data/perf/canvas_bench': None,
    'src/chrome/test/data/perf/frame_rate/content': None,
    'src/native_client': None,
    'src/third_party/ffmpeg': None,
    'src/third_party/hunspell_dictionaries': None,
    'src/third_party/hunspell': None,
    'src/third_party/webgl': None,
  },
  'mac': {
    'src/chrome/tools/test/reference_build/chrome_mac':
     Var('chromium_git') + '/chromium/reference_builds/chrome_mac.git' + '@' + '8dc181329e7c5255f83b4b85dc2f71498a237955',

    'src/third_party/google_toolbox_for_mac/src':
      Var('chromium_git') + '/external/google-toolbox-for-mac.git' + '@' + Var('google_toolbox_for_mac_revision'),


    'src/third_party/pdfsqueeze':
      Var('chromium_git') + '/external/pdfsqueeze.git' + '@' + '5936b871e6a087b7e50d4cbcb122378d8a07499f',

    'src/third_party/lighttpd':
     Var('chromium_git') + '/chromium/deps/lighttpd.git' + '@' + Var('lighttpd_revision'),

    'src/third_party/swig/mac':
     Var('chromium_git') + '/chromium/deps/swig/mac.git' + '@' + '1b182eef16df2b506f1d710b34df65d55c1ac44e',

    # NSS, for SSLClientSocketNSS.
    'src/third_party/nss':
     Var('chromium_git') + '/chromium/deps/nss.git' + '@' + Var('nss_revision'),

    'src/chrome/installer/mac/third_party/xz/xz':
     Var('chromium_git') + '/chromium/deps/xz.git' + '@' + 'eecaf55632ca72e90eb2641376bce7cdbc7284f7',
  },
  'unix': {
    # Linux, really.
    'src/chrome/tools/test/reference_build/chrome_linux':
     Var('chromium_git') + '/chromium/reference_builds/chrome_linux64.git' + '@' + '033d053a528e820e1de3e2db766678d862a86b36',

    'src/third_party/xdg-utils':
     Var('chromium_git') + '/chromium/deps/xdg-utils.git' + '@' + 'd80274d5869b17b8c9067a1022e4416ee7ed5e0d',

    'src/third_party/swig/linux':
     Var('chromium_git') + '/chromium/deps/swig/linux.git' + '@' + '866b8e0e0e0cfe99ebe608260030916ca0c3f92d',

    'src/third_party/lss':
      Var('chromium_git') + '/external/linux-syscall-support/lss.git' + '@' + Var('lss_revision'),

    # For Linux and Chromium OS.
    'src/third_party/cros_system_api':
     Var('chromium_git') + '/chromiumos/platform/system_api.git' + '@' + '95c486f3d3da2cb4e6564f1cd69ab522cf315ea1',

    # Note that this is different from Android's freetype repo.
    'src/third_party/freetype2/src':
     Var('chromium_git') + '/chromium/src/third_party/freetype2.git' + '@' + '495a23fce9cd125f715dc20643d14fed226d76ac',

    # Build tools for Chrome OS.
    'src/third_party/chromite':
     Var('chromium_git') + '/chromiumos/chromite.git' + '@' + 'fdc9440cb96f8de35202abc285ffb896e04292d3',

    # Dependency of chromite.git.
    'src/third_party/pyelftools':
     Var('chromium_git') + '/chromiumos/third_party/pyelftools.git' + '@' + '19b3e610c86fcadb837d252c794cb5e8008826ae',

    'src/third_party/undoview':
     Var('chromium_git') + '/chromium/deps/undoview.git' + '@' + '3ba503e248f3cdbd81b78325a24ece0984637559',

    'src/third_party/liblouis/src':
     Var('chromium_git') + '/external/liblouis-github.git' + '@' + '5f9c03f2a3478561deb6ae4798175094be8a26c2',

    # Used for embedded builds. CrOS & Linux use the system version.
    'src/third_party/fontconfig/src':
     Var('chromium_git') + '/external/fontconfig.git' + '@' + 'f16c3118e25546c1b749f9823c51827a60aeb5c1',

    'src/third_party/stp/src':
     Var('chromium_git') + '/external/github.com/stp/stp.git' + '@' + 'fc94a599207752ab4d64048204f0c88494811b62',
  },
  'android': {
    'src/third_party/android_protobuf/src':
     Var('chromium_git') + '/external/android_protobuf.git' + '@' + '94f522f907e3f34f70d9e7816b947e62fddbb267',

    'src/third_party/android_tools':
     Var('chromium_git') + '/android_tools.git' + '@' + '98a434576d8d9a65f9bdb8ea161e158142c0c5e5',

    'src/third_party/apache-mime4j':
     Var('chromium_git') + '/chromium/deps/apache-mime4j.git' + '@' + '28cb1108bff4b6cf0a2e86ff58b3d025934ebe3a',

    'src/third_party/appurify-python/src':
     Var('chromium_git') + '/external/github.com/appurify/appurify-python.git' + '@' + 'ee7abd5c5ae3106f72b2a0b9d2cb55094688e867',

    'src/third_party/findbugs':
     Var('chromium_git') + '/chromium/deps/findbugs.git' + '@' + '7f69fa78a6db6dc31866d09572a0e356e921bf12',

    'src/third_party/freetype':
     Var('chromium_git') + '/chromium/src/third_party/freetype.git' + '@' + 'fd6919ac23f74b876c209aba5eaa2be662086391',

   'src/third_party/elfutils/src':
     Var('chromium_git') + '/external/elfutils.git' + '@' + '249673729a7e5dbd5de4f3760bdcaa3d23d154d7',

    'src/third_party/httpcomponents-client':
     Var('chromium_git') + '/chromium/deps/httpcomponents-client.git' + '@' + '285c4dafc5de0e853fa845dce5773e223219601c',

    'src/third_party/httpcomponents-core':
     Var('chromium_git') + '/chromium/deps/httpcomponents-core.git' + '@' + '9f7180a96f8fa5cab23f793c14b413356d419e62',

    'src/third_party/jarjar':
     Var('chromium_git') + '/chromium/deps/jarjar.git' + '@' + '2e1ead4c68c450e0b77fe49e3f9137842b8b6920',

    'src/third_party/jsr-305/src':
      Var('chromium_git') + '/external/jsr-305.git' + '@' + '642c508235471f7220af6d5df2d3210e3bfc0919',

    'src/third_party/junit/src':
      Var('chromium_git') + '/external/junit.git' + '@' + '45a44647e7306262162e1346b750c3209019f2e1',

    'src/third_party/mockito/src':
      Var('chromium_git') + '/external/mockito/mockito.git' + '@' + 'ed99a52e94a84bd7c467f2443b475a22fcc6ba8e',

    'src/third_party/robolectric/lib':
      Var('chromium_git') + '/chromium/third_party/robolectric.git' + '@' + '6b63c99a8b6967acdb42cbed0adb067c80efc810',

    'src/third_party/lss':
      Var('chromium_git') + '/external/linux-syscall-support/lss.git' + '@' + Var('lss_revision'),

    'src/third_party/requests/src':
      Var('chromium_git') + '/external/github.com/kennethreitz/requests.git' + '@' + 'f172b30356d821d180fa4ecfa3e71c7274a32de4',

  },
}


include_rules = [
  # Everybody can use some things.
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
  'breakpad',
  'delegate_execute',
  'metro_driver',
  'native_client_sdk',
  'o3d',
  'sdch',
  'skia',
  'testing',
  'third_party',
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
    # This downloads binaries for Native Client's newlib toolchain.
    # Done in lieu of building the toolchain from scratch as it can take
    # anywhere from 30 minutes to 4 hours depending on platform to build.
    'name': 'nacltools',
    'pattern': '.',
    'action': [
        'python',
        'src/build/download_nacl_toolchains.py',
        '--mode', 'nacl_core_sdk',
        'sync', '--extract',
    ],
  },
  {
    # This downloads SDK extras and puts them in the
    # third_party/android_tools/sdk/extras directory on the bots. Developers
    # need to manually install these packages and accept the ToS.
    'name': 'sdkextras',
    'pattern': '.',
    # When adding a new sdk extras package to download, add the package
    # directory and zip file to .gitignore in third_party/android_tools.
    'action': ['python', 'src/build/download_sdk_extras.py'],
  },
  {
    # Downloads the Debian Wheezy sysroot to chrome/installer/linux if needed.
    # This sysroot updates at about the same rate that the chrome build deps
    # change. This script is a no-op except for linux users who are doing
    # official chrome builds.
    'name': 'sysroot',
    'pattern': '.',
    'action': [
        'python',
        'src/chrome/installer/linux/sysroot_scripts/install-debian.wheezy.sysroot.py',
        '--linux-only'],
  },
  {
    # Update the Windows toolchain if necessary.
    'name': 'win_toolchain',
    'pattern': '.',
    'action': ['python', 'src/build/vs_toolchain.py', 'update'],
  },
  {
    # Pull clang if needed or requested via GYP_DEFINES.
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python', 'src/tools/clang/scripts/update.py', '--if-needed'],
  },
  {
    # Update LASTCHANGE. This is also run by export_tarball.py in
    # src/tools/export_tarball - please keep them in sync.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python', 'src/build/util/lastchange.py',
               '-o', 'src/build/util/LASTCHANGE'],
  },
  {
    # Update LASTCHANGE.blink. This is also run by export_tarball.py in
    # src/tools/export_tarball - please keep them in sync.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python', 'src/build/util/lastchange.py',
               '-s', 'src/third_party/WebKit',
               '-o', 'src/build/util/LASTCHANGE.blink'],
  },
  # Pull GN binaries. This needs to be before running GYP below.
  {
    'name': 'gn_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'src/buildtools/win/gn.exe.sha1',
    ],
  },
  {
    'name': 'gn_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'src/buildtools/mac/gn.sha1',
    ],
  },
  {
    'name': 'gn_linux32',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'src/buildtools/linux32/gn.sha1',
    ],
  },
  {
    'name': 'gn_linux64',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'src/buildtools/linux64/gn.sha1',
    ],
  },
  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'src/buildtools/win/clang-format.exe.sha1',
    ],
  },
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'src/buildtools/mac/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'src/buildtools/linux64/clang-format.sha1',
    ],
  },
  # Pull binutils for linux, enabled debug fission for faster linking /
  # debugging when used with clang on Ubuntu Precise.
  # https://code.google.com/p/chromium/issues/detail?id=352046
  {
    'name': 'binutils',
    'pattern': 'src/third_party/binutils',
    'action': [
        'python',
        'src/third_party/binutils/download.py',
    ],
  },
  # Pull eu-strip binaries using checked-in hashes.
  {
    'name': 'eu-strip',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-eu-strip',
                '-s', 'src/build/linux/bin/eu-strip.sha1',
    ],
  },
  {
    'name': 'drmemory',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-drmemory',
                '-s', 'src/third_party/drmemory/drmemory-windows-sfx.exe.sha1',
              ],
  },
  # Pull the Syzygy binaries, used for optimization and instrumentation.
  {
    'name': 'syzygy-binaries',
    'pattern': '.',
    'action': ['python',
               'src/build/get_syzygy_binaries.py',
               '--output-dir=src/third_party/syzygy/binaries',
               '--revision=e869aa03e0f40f74321a70d6d4578f5bbbc29bda',
               '--overwrite',
    ],
  },
  {
    'name': 'kasko',
    'pattern': '.',
    'action': ['python',
               'src/build/get_syzygy_binaries.py',
               '--output-dir=src/third_party/kasko',
               '--revision=3278b959bdf6c9aa4b326a38e8b953e566c1ec58',
               '--resource=kasko.zip',
               '--resource=kasko_symbols.zip',
               '--overwrite',
    ],
  },
  {
    'name': 'apache_win32',
    'pattern': '\\.sha1',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--directory',
                '--recursive',
                '--no_auth',
                '--num_threads=16',
                '--bucket', 'chromium-apache-win32',
                'src/third_party/apache-win32',
    ],
  },
  # Pull the mojo_shell binary, used for mojo development
  {
    'name': 'download_mojo_shell',
    'pattern': '',
    'action': [ 'python',
                'src/third_party/mojo/src/mojo/public/tools/download_shell_binary.py',
                '--tools-directory=../../../../../../tools',
              ],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    'name': 'gyp',
    'pattern': '.',
    'action': ['python', 'src/build/gyp_chromium'],
  },
  {
    # Verify committers' ~/.netc, gclient and git are properly configured for
    # write access to the git repo. To be removed sometime after Chrome to git
    # migration completes (let's say Sep 1 2014).
    'name': 'check_git_config',
    'pattern': '.',
    'action': [
        'python',
        'src/tools/check_git_config.py',
        '--running-as-hook',
    ],
  },
  {
    # Ensure that we don't accidentally reference any .pyc files whose
    # corresponding .py files have already been deleted.
    'name': 'remove_stale_pyc_files',
    'pattern': 'src/tools/.*\\.py',
    'action': [
        'python',
        'src/tools/remove_stale_pyc_files.py',
        'src/tools',
    ],
  },
]
