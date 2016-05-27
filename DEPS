# DEPS files look like -*- Python -*-

vars = {
  # These revisions are slices of the chromium repository.
  # Because they come from separate sub-slices, their hashes do not match at
  # equivalent revisions. When updating them, use the roll-dep script
  # to move them to equivalent revisions. Additionally, because not all
  # directories contain commits at each revision, you will need to select
  # revisions at latest revision up to a high watermark from each slice.
  # Document the high watermark here:
  # chrome_rev: 396528
  "build_rev": "a3802f0af222b2bcce4925e8df9b019f5a7f0263", # from cr commit position 396479
  "binutils_rev": "8d77853bc9415bcb7bb4206fa2901de7603387db", # from cr commit position 392828
  "libcxx_static_rev": "fd8f9a82862aa40dab0c5bcdea6e498715284f5a", # from cr commit position 369150
  # NOTE: be sure to update clang_lib_version in SConstruct whenever
  # updating this clang_rev (e.g., if LLVM changes from version 3.7 to 3.8).
  "clang_rev": "ef8e028ea0f0fdf3be7be6e817e5c26c8ba7aebe", # from cr commit position 396388

  # NOTE!  These five should be kept up to date with their counterparts in
  # chromium/src/DEPS.
  # Be sure to update them when updating the chromium slice revisions above.
  # (This is not essential for Breakpad, because we do not use its code
  # in the build that goes into Chromium.  But we might as well update it too.)
  # You should now use the roll-dep script in depot_tools to do this update.
  "gtest_rev": "6f8a66431cb592dad629028a50b3dd418a408c87",
  "gyp_rev": "bce1c7793010574d88d7915e2d55395213ac63d1",
  "breakpad_rev": "a4eb2e302cefff9908ec955e761fef5d813d1b00", # from svn revision 1416
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling build tools
  # and whatever else without interference from each other.
  'buildtools_revision': '06e80a0e17319868d4a9b13f9bb6a248dc8d8b20',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling lss
  # and whatever else without interference from each other.
  'lss_revision': '4fc942258fe5509549333b9487ec018e3c8c5b10',

  # Separately pinned repositories, update with roll-dep individually.
  "third_party_rev": "d4e38e5faf600b39649025e5605d6e7f94518ea7",
  "lcov_rev": "b37daf5968200da8ff520ce65c4e5bce4047dd15", # from svn revision 149720
  "gnu_binutils_rev": "f4003433b61b25666565690caf3d7a7a1a4ec436", # from svn revision 8151
  "mingw_rev": "3cc8b140b883a9fe4986d12cfd46c16a093d3527", # from svn revision 7064
  "nsis_rev": "21b6ad22daa7bfc04b9f1c1805a34622e2607a93", # from svn revision 7071
  "ragel_rev": "da42bb33f1b67c2d70b38ec1d2edf5263271b635", # from svn revision 9010
  "validator_snapshots_rev": "ef053694ef9b0d98d9bed0b9bb649963084bfc81",
  # Rolls of build_overrides_rev should done at the same time as a roll of
  # build_rev as build_overrides/ is tightly coupled with build/.
  "build_overrides_rev": "2881125066dcf38dae595d92eac8a6b477529116",

  "chromium_git": "https://chromium.googlesource.com",
}

deps = {
  "breakpad":
    Var("chromium_git") + "/external/google-breakpad.git@" +
    Var("breakpad_rev"),
  "buildtools":
    Var("chromium_git") + "/chromium/buildtools.git@" +
     Var("buildtools_revision"),
  "build":
    Var("chromium_git") + "/chromium/src/build.git@" +
    Var("build_rev"),
  "build_overrides":
    Var("chromium_git") + "/native_client/src/build_overrides.git@" +
    Var("build_overrides_rev"),
  "testing/gtest":
    (Var("chromium_git") + "/external/github.com/google/googletest.git@" +
     Var("gtest_rev")),
  "third_party":
    Var("chromium_git") + "/native_client/src/third_party.git@" +
    Var("third_party_rev"),
  "validator_snapshots":
    (Var("chromium_git") + "/native_client/src/validator_snapshots.git@" +
     Var("validator_snapshots_rev")),
  "third_party/binutils":
    Var("chromium_git") + "/chromium/src/third_party/binutils.git@" +
    Var("binutils_rev"),
  "third_party/lcov":
    Var("chromium_git") + "/chromium/src/third_party/lcov.git@" +
    Var("lcov_rev"),
  "third_party/libc++-static":
    Var("chromium_git") + "/chromium/src/third_party/libc++-static.git@" +
    Var("libcxx_static_rev"),
  "third_party/lss":
    Var("chromium_git") + "/external/linux-syscall-support/lss.git@" +
    Var("lss_revision"),
  "tools/clang":
    Var("chromium_git") + "/chromium/src/tools/clang.git@" + Var("clang_rev"),
  "tools/gyp":
    Var("chromium_git") + "/external/gyp.git@" + Var("gyp_rev"),
}

deps_os = {
  "win": {
    # GNU binutils assembler for x86-32.
    "third_party/gnu_binutils":
      Var("chromium_git") +
      "/native_client/deps/third_party/gnu_binutils.git@" +
      Var("gnu_binutils_rev"),
    # GNU binutils assembler for x86-64.
    "third_party/mingw-w64/mingw/bin":
      Var("chromium_git") +
      "/native_client/deps/third_party/mingw-w64/mingw/bin.git@" +
      Var("mingw_rev"),
    "third_party/NSIS":
      Var("chromium_git") + "/native_client/deps/third_party/NSIS.git@" +
      Var("nsis_rev"),
  },
  "unix": {
    # Ragel for validator_ragel
    "third_party/ragel":
      Var("chromium_git") + "/native_client/deps/third_party/ragel.git@" +
      Var("ragel_rev"),
  },
}

hooks = [
  ###
  ### From here until the similar marker below, these clauses are copied
  ### almost verbatim from chromium/src/DEPS.  They are modified to drop
  ### the src/ prefix on file names, and third_party/binutils/download.py
  ### gets the extra --ignore-if-arch=arm argument, but otherwise they
  ### should stay identical.
  ###

  {
    # Downloads the current stable linux sysroot to build/linux/ if needed.
    # This sysroot updates at about the same rate that the chrome build deps
    # change. This script is a no-op except for linux users who are doing
    # official chrome builds or cross compiling.
    'name': 'sysroot',
    'pattern': '.',
    'action': ['python', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--running-as-hook'],
  },
  {
    # Update the Windows toolchain if necessary.
    'name': 'win_toolchain',
    'pattern': '.',
    'action': ['python', 'build/vs_toolchain.py', 'update'],
  },
  # Pull binutils for linux, enabled debug fission for faster linking /
  # debugging when used with clang on Ubuntu Precise.
  # https://code.google.com/p/chromium/issues/detail?id=352046
  {
    'name': 'binutils',
    'pattern': 'third_party/binutils',
    'action': [
        'python',
        'third_party/binutils/download.py',
        '--ignore-if-arch=arm',
    ],
  },
  {
    # Pull clang if needed or requested via GYP_DEFINES.
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python', 'tools/clang/scripts/update.py', '--if-needed'],
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
                '-s', 'buildtools/win/gn.exe.sha1',
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
                '-s', 'buildtools/mac/gn.sha1',
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
                '-s', 'buildtools/linux64/gn.sha1',
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
                '-s', 'buildtools/win/clang-format.exe.sha1',
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
                '-s', 'buildtools/mac/clang-format.sha1',
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
                '-s', 'buildtools/linux64/clang-format.sha1',
    ],
  },
  # Pull the prebuilt libc++ static library for mac.
  {
    'name': 'libcpp_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-libcpp',
                '-s', 'third_party/libc++-static/libc++.a.sha1',
    ],
  },

  ###
  ### End of clauses copied (almost verbatim) from chromium/src/DEPS.
  ###

  # Pull NaCl Toolchain binaries.
  {
    "pattern": ".",
    "action": ["python",
               "native_client/build/package_version/package_version.py",
               "sync", "--extract",
    ],
  },
  # Cleanup any stale package_version files.
  {
    "pattern": ".",
    "action": ["python",
               "native_client/build/package_version/package_version.py",
               "cleanup",
    ],
  },
]

include_rules = [
  "+native_client/src/include",
  "+gtest",
]
