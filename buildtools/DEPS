use_relative_paths = True

vars = {
  'chromium_url': 'https://chromium.googlesource.com',

  #
  # TODO(crbug.com/941824): These revisions need to be kept in sync
  # between //DEPS and //buildtools/DEPS, so if you're updating one,
  # update the other. There is a presubmit check that checks that
  # you've done so; if you are adding new tools to //buildtools and
  # hence new revisions to this list, make sure you update the
  # _CheckBuildtoolsRevsAreInSync in PRESUBMIT.py to include the additional
  # revisions.
  #

  # When changing these, also update the svn revisions in deps_revisions.gni
  'clang_format_revision': '96636aa0e9f047f17447f2d45a094d0b59ed7917',
  'libcxx_revision':       'a50f5035629b7621e92acef968403f71b7d48553',
  'libcxxabi_revision':    '0d529660e32d77d9111912d73f2c74fc5fa2a858',
  'libunwind_revision':    '69d9b84cca8354117b9fe9705a4430d789ee599b',
}

deps = {
  'clang_format/script':
    Var('chromium_url') + '/chromium/llvm-project/cfe/tools/clang-format.git@' +
    Var('clang_format_revision'),
  'third_party/libc++/trunk':
    Var('chromium_url') + '/chromium/llvm-project/libcxx.git' + '@' +
    Var('libcxx_revision'),
  'third_party/libc++abi/trunk':
    Var('chromium_url') + '/chromium/llvm-project/libcxxabi.git' + '@' +
    Var('libcxxabi_revision'),
  'third_party/libunwind/trunk':
    Var('chromium_url') + '/external/llvm.org/libunwind.git' + '@' +
    Var('libunwind_revision'),
}
