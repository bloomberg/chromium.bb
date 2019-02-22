# This file is used to manage the dependencies of the Open Screen repo. It is
# used by gclient to determine what version of each dependency to check out.
#
# For more information, please refer to the official documentation:
#   https://sites.google.com/a/chromium.org/dev/developers/how-tos/get-the-code
#
# When adding a new dependency, please update the top-level .gitignore file
# to list the dependency's destination directory.

vars = {
    'boringssl_git': 'https://boringssl.googlesource.com',
    'chromium_git': 'https://chromium.googlesource.com',

    # TODO(jophba): move to googlesource external for github repos.
    'github': "https://github.com",
    'third_party_dir': 'openscreen/third_party'
}

deps = {
    Var('third_party_dir') + '/googletest/src':
        Var('chromium_git') + '/external/github.com/google/googletest.git' +
        '@' + 'dfa853b63d17c787914b663b50c2095a0c5b706e',

    Var('third_party_dir') + '/mDNSResponder/src':
        Var('github') + '/jevinskie/mDNSResponder.git' +
        '@' + '2942dde61f920fbbf96ff9a3840567ebbe7cb1b6',

    Var('third_party_dir') + '/chromium_quic/src/base':
        Var('chromium_git') + '/chromium/src/base' +
        '@' + '6abc23a6273fa803f6f633ad0c77358998db0489',

    Var('third_party_dir') + '/chromium_quic/src/third_party/boringssl/src':
        Var('boringssl_git') + '/boringssl.git' +
        '@' + '6410e18e9190b6b0c71955119fbf3cae1b9eedb7',

    Var('third_party_dir') + '/tinycbor/src':
        Var('github') + '/intel/tinycbor.git' +
        '@' + 'bfc40dcf909f1998d7760c2bc0e1409979d3c8cb',

    Var('third_party_dir') + '/abseil/src':
        Var('chromium_git') +
        '/external/github.com/abseil/abseil-cpp.git' +
        '@' + '5eea0f713c14ac17788b83e496f11903f8e2bbb0',
}
