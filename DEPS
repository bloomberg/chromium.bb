# This file is used to manage the dependencies of the Open Screen repo. It is
# used by gclient to determine what version of each dependency to check out.
#
# For more information, please refer to the official documentation:
#   https://sites.google.com/a/chromium.org/dev/developers/how-tos/get-the-code
#
# When adding a new dependency, please update the top-level .gitignore file
# to list the dependency's destination directory.

use_relative_paths = True

vars = {
    'boringssl_git': 'https://boringssl.googlesource.com',
    'chromium_git': 'https://chromium.googlesource.com',

    # TODO(jophba): move to googlesource external for github repos.
    'github': 'https://github.com',

    # NOTE: Strangely enough, this will be overridden by any _parent_ DEPS, so
    # in Chromium it will correctly be True.
    'build_with_chromium': False,

    'gn_version': 'git_revision:0790d3043387c762a6bacb1ae0a9ebe883188ab2',
    'checkout_chromium_quic_boringssl': False,

    # By default, do not check out openscreen/cast. This can be overridden
    # by custom_vars in .gclient.
    'checkout_openscreen_cast_internal': False
}

deps = {
    'cast/internal': {
        'url': 'https://chrome-internal.googlesource.com/openscreen/cast.git' +
            '@' + '703984f9d1674c2cfc259904a5a7fba4990cca4b',
        'condition': 'checkout_openscreen_cast_internal',
    },

    'buildtools': {
        'url': Var('chromium_git')+ '/chromium/src/buildtools' +
            '@' + 'd5c58b84d50d256968271db459cd29b22bff1ba2',
        'condition': 'not build_with_chromium',
    },

    'buildtools/linux64': {
        'packages': [
            {
                'package': 'gn/gn/linux-amd64',
                'version': Var('gn_version'),
            },
        ],
        'dep_type': 'cipd',
        'condition': 'checkout_linux',
    },

    'buildtools/mac': {
        'packages': [
            {
                'package': 'gn/gn/mac-amd64',
                'version': Var('gn_version'),
            },
        ],
        'dep_type': 'cipd',
        'condition': 'checkout_mac',
    },

    'third_party/protobuf/src': {
        'url': Var('chromium_git') +
            '/external/github.com/protocolbuffers/protobuf.git' +
            '@' + '09745575a923640154bcf307fba8aedff47f240a', # version 3.8.0
        'condition': 'not build_with_chromium',
    },

    'third_party/zlib/src': {
        'url': Var('github') +
            '/madler/zlib.git' +
            '@' + 'cacf7f1d4e3d44d871b605da3b647f07d718623f', # version 1.2.11
        'condition': 'not build_with_chromium',
    },

    'third_party/jsoncpp/src': {
        'url': Var('chromium_git') +
            '/external/github.com/open-source-parsers/jsoncpp.git' +
            '@' + '3c32dca89214c03b107cc9d1c468000cff3f8127', # version 1.9.0
        'condition': 'not build_with_chromium',
    },

    'third_party/googletest/src': {
        'url': Var('chromium_git') +
            '/external/github.com/google/googletest.git' +
            '@' + 'dfa853b63d17c787914b663b50c2095a0c5b706e',
        'condition': 'not build_with_chromium',
    },

    'third_party/mDNSResponder/src': {
        'url': Var('github') + '/jevinskie/mDNSResponder.git' +
            '@' + '2942dde61f920fbbf96ff9a3840567ebbe7cb1b6',
        'condition': 'not build_with_chromium',
    },

    'third_party/boringssl/src': {
        'url' : Var('boringssl_git') + '/boringssl.git' +
            '@' + '6410e18e9190b6b0c71955119fbf3cae1b9eedb7',
        'condition': 'not build_with_chromium',
    },

    'third_party/chromium_quic/src': {
        'url': Var('chromium_git') + '/openscreen/quic.git' +
            '@' + 'd60d1b9e6f24553900f79043ff127a9a649281c4',
        'condition': 'not build_with_chromium',
    },

    'third_party/tinycbor/src':
        Var('chromium_git') + '/external/github.com/intel/tinycbor.git' +
        '@' + 'bfc40dcf909f1998d7760c2bc0e1409979d3c8cb',

    'third_party/abseil/src': {
        'url': Var('chromium_git') +
            '/external/github.com/abseil/abseil-cpp.git' +
            '@' + '5eea0f713c14ac17788b83e496f11903f8e2bbb0',
        'condition': 'not build_with_chromium',
    },
}

recursedeps = [
    'third_party/chromium_quic/src',
]

include_rules = [
    '+util',
    '+platform',
    '+third_party',

    # Don't include abseil from the root so the path can change via include_dirs
    # rules when in Chromium.
    '-third_party/abseil',

    # Abseil whitelist.
    '+absl/algorithm/container.h',
    '+absl/base/thread_annotations.h',
    '+absl/hash/hash.h',
    '+absl/strings/ascii.h',
    '+absl/strings/match.h',
    '+absl/strings/numbers.h',
    '+absl/strings/str_cat.h',
    '+absl/strings/str_join.h',
    '+absl/strings/string_view.h',
    '+absl/strings/substitute.h',
    '+absl/types/optional.h',
    '+absl/types/span.h',
    '+absl/types/variant.h',

    # Similar to abseil, don't include boringssl using root path.  Instead,
    # explicitly allow 'openssl' where needed.
    '-third_party/boringssl',

    # Test framework includes.
    "-third_party/googletest",
    "+gtest",
    "+gmock",
]

skip_child_includes = [
    'third_party/chromium_quic',
]
