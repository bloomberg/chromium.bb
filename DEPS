# DEPS files look like -*- Python -*-

vars = {
  # These revisions are slices of the chromium repository.
  # Because they come from separate sub-slices, their hashes do not match at
  # equivalent revisions. When updating them, use the roll-dep script
  # to move them to equivalent revisions. Additionally, because not all
  # directories contain commits at each revision, you will need to select
  # revisions at latest revision up to a high watermark from each slice.
  # Document the high watermark here:
  # chrome_rev: 745019
  "build_rev": "716de5b7e1f94a1a7e056a02c3f48d99afb9145f", # from cr commit position 745019
  "binutils_rev": "8d77853bc9415bcb7bb4206fa2901de7603387db", # from cr commit position 392828
  # NOTE: be sure to update clang_lib_version in SConstruct whenever
  # updating this clang_rev (e.g., if LLVM changes from version 3.7 to 3.8).
  "clang_rev": "f5a394169f59ae2bb15966b2ef6388fe17b3e166", # from cr commit position 744792

  # Rolls of build_overrides_rev should done at the same time as a roll
  # of build_rev as build_overrides/ is tightly coupled with build/.
  # build_overrides/ is forked from chromium/src/build_overrides/ and
  # needs to be manually updated to keep in sync before a build_rev
  # update can be done.
  "build_overrides_rev": "d56d0aa73300038348206b7503934d3706131d09",

  # NOTE!  These five should be kept up to date with their counterparts in
  # chromium/src/DEPS.
  # Be sure to update them when updating the chromium slice revisions above.
  # (This is not essential for Breakpad, because we do not use its code
  # in the build that goes into Chromium.  But we might as well update it too.)
  # You should now use the roll-dep script in depot_tools to do this update.
  "gtest_rev": "2d3543f81d6d4583332f8b60768ade18e0f96220",
  "gyp_rev": "e7079f0e0e14108ab0dba58728ff219637458563",
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling build tools
  # and whatever else without interference from each other.
  'buildtools_revision': 'afc5b798c72905e85f9991152be878714c579958',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling lss
  # and whatever else without interference from each other.
  'lss_revision': '3f6478ac95edf86cd3da300c2c0d34a438f5dbeb',

  "breakpad_rev": "54fa71efbe50fb2b58096d871575b59e12edba6d",

  # GN CIPD package version.
  'gn_version': 'git_revision:97cc440d84f050f99ff0161f9414bfa2ffa38f65',

  # Separately pinned repositories, update with roll-dep individually.
  "third_party_rev": "d4e38e5faf600b39649025e5605d6e7f94518ea7",
  "lcov_rev": "b37daf5968200da8ff520ce65c4e5bce4047dd15", # from svn revision 149720
  "gnu_binutils_rev": "f4003433b61b25666565690caf3d7a7a1a4ec436", # from svn revision 8151
  "mingw_rev": "3cc8b140b883a9fe4986d12cfd46c16a093d3527", # from svn revision 7064
  "nsis_rev": "21b6ad22daa7bfc04b9f1c1805a34622e2607a93", # from svn revision 7071
  "ragel_rev": "da42bb33f1b67c2d70b38ec1d2edf5263271b635", # from svn revision 9010
  "validator_snapshots_rev": "ef053694ef9b0d98d9bed0b9bb649963084bfc81",

  "chromium_git": "https://chromium.googlesource.com",
}

deps = {
  "breakpad":
    Var("chromium_git") + "/breakpad/breakpad.git@" +
    Var("breakpad_rev"),
  "buildtools":
    Var("chromium_git") + "/chromium/src/buildtools.git@" +
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
  "third_party/lss":
    Var("chromium_git") + "/linux-syscall-support.git@" +
    Var("lss_revision"),
  "tools/clang":
    Var("chromium_git") + "/chromium/src/tools/clang.git@" + Var("clang_rev"),
  "tools/gyp":
    Var("chromium_git") + "/external/gyp.git@" + Var("gyp_rev"),

  'src/buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "linux"',
  },
  'buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "mac"',
  },
  'buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "win"',
  },
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
               '--all'],
  },
  {
    # Update the Windows toolchain if necessary.
    'name': 'win_toolchain',
    'pattern': '.',
    'action': ['python', 'build/vs_toolchain.py', 'update'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'checkout_mac',
    'action': ['python', 'build/mac_toolchain.py'],
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
    'action': ['python', 'tools/clang/scripts/update.py'],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python', 'build/util/lastchange.py',
               '-o', 'build/util/LASTCHANGE'],
  },
  {
    # Verify that we have the right GN binary and force-install it if we
    # don't, in order to work around crbug.com/944367.
    # TODO(crbug.com/944667) Get rid of this when cipd is ensuring we
    # have the right binary more carefully and we no longer need this.
    'name': 'ensure_gn_version',
    'pattern': '.',
    'action': [
      'python',
      'buildtools/ensure_gn_version.py',
      Var('gn_version')
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
