# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configuration options for cbuildbot boards."""

from __future__ import print_function

#
# Define assorted constants describing various sets of boards.
#

# Base per-board configuration.
# Every board must appear in exactly 1 of the following sets.

#
# Define assorted constants describing various sets of boards.
#

# Base per-board configuration.
# Every board must appear in exactly 1 of the following sets.

arm_internal_release_boards = frozenset([
    'arkham',
    'beaglebone',
    'beaglebone_servo',
    'bob',
    'bubs',
    'capri',
    'capri-zfpga',
    'cheza',
    'cheza64',
    'cobblepot',
    'elm',
    'flapjack',
    'gale',
    'gonzo',
    'hana',
    'jacuzzi',
    'kevin',
    'kevin64',
    'kevin-arc64',
    'kevin-arcnext',
    'kukui',
    'lasilla-ground',
    'littlejoe',
    'mistral',
    'nyan_big',
    'nyan_blaze',
    'nyan_kitty',
    'oak',
    'octavius',
    'romer',
    'scarlet',
    'veyron_fievel',
    'veyron_jaq',
    'veyron_jerry',
    'veyron_mickey',
    'veyron_mighty',
    'veyron_minnie',
    'veyron_rialto',
    'veyron_speedy',
    'veyron_tiger',
    'viking',
    'whirlwind',
    'wooten',
])

arm_external_boards = frozenset([
    'arm-generic',
    'arm64-generic',
    'arm64-llvmpipe',
    'tael',
])

x86_internal_release_boards = frozenset([
    'amd64-generic-cheets',
    'asuka',
    'atlas',
    'auron_paine',
    'auron_yuna',
    'banjo',
    'banon',
    'betty',
    'betty-arc64',
    'betty-arcnext',
    'betty-pi-arc',
    'betty-qt-arc',
    'buddy',
    'candy',
    'caroline',
    'cave',
    'celes',
    'chell',
    'coral',
    'cyan',
    'drallion',
    'edgar',
    'enguarde',
    'eve',
    'eve-arcnext',
    'eve-arcvm',
    'excelsior',
    'expresso',
    'falco_li',
    'fizz',
    'fizz-accelerator',
    'fizz-moblab',
    'fizz-labstation',
    'gandof',
    'glados',
    'gnawty',
    'grunt',
    'guado',
    'guado-accelerator',
    'guado_labstation',
    'hatch',
    'heli',
    'jecht',
    'kalista',
    'kefka',
    'kefka-kernelnext',
    'kip',
    'kumo',
    'lakitu',
    'lakitu-gpu',
    'lakitu-nc',
    'lakitu-st',
    'lakitu_next',
    'lars',
    'lulu',
    'monroe',
    'nami',
    'nautilus',
    'ninja',
    'nocturne',
    'novato',
    'novato-arc64',
    'octopus',
    'orco',
    'poppy',
    'puff',
    'pyro',
    'rammus',
    'reef',
    'reks',
    'relm',
    'rikku',
    'samus',
    'samus-kernelnext',
    'sand',
    'sarien',
    'sentry',
    'setzer',
    'sludge',
    'snappy',
    'soraka',
    'sumo',
    'swanky',
    'terra',
    'tidus',
    'ultima',
    'volteer',
    'winky',
    'wizpig',
    'wristpin',
    'zork',
])

x86_external_boards = frozenset([
    'amd64-generic',
    'moblab-generic-vm',
    'tatl',
    'x32-generic',
])

# Board can appear in 1 or more of the following sets.
brillo_boards = frozenset([
    'arkham',
    'gale',
    'mistral',
    'whirlwind',
])

accelerator_boards = frozenset([
    'fizz-accelerator',
    'guado-accelerator',
])

beaglebone_boards = frozenset([
    'beaglebone',
    'beaglebone_servo',
])

dustbuster_boards = frozenset([
    'wristpin',
])

lakitu_boards = frozenset([
    # Although its name doesn't indicate any lakitu relevance,
    # kumo board is developed by the lakitu-dev team.
    'kumo',
    'lakitu',
    'lakitu-gpu',
    'lakitu-nc',
    'lakitu-st',
    'lakitu_next',
])

lassen_boards = frozenset([
    'lassen',
])

loonix_boards = frozenset([
    'capri',
    'capri-zfpga',
    'cobblepot',
    'gonzo',
    'lasilla-ground',
    'octavius',
    'romer',
    'wooten',
])

wshwos_boards = frozenset([
    'littlejoe',
    'viking',
])

moblab_boards = frozenset([
    'fizz-moblab',
    'moblab-generic-vm',
])

scribe_boards = frozenset([
    'guado-macrophage',
])

termina_boards = frozenset([
    'sludge',
    'tatl',
    'tael',
])

nofactory_boards = (
    lakitu_boards | termina_boards | lassen_boards | frozenset([
        'x30evb',
    ])
)

toolchains_from_source = frozenset([
    'x32-generic',
])

noimagetest_boards = (lakitu_boards | loonix_boards | termina_boards
                      | scribe_boards | wshwos_boards | dustbuster_boards)

nohwqual_boards = (lakitu_boards | lassen_boards | loonix_boards
                   | termina_boards | beaglebone_boards | wshwos_boards
                   | dustbuster_boards)

norootfs_verification_boards = frozenset([
    'kumo',
])

base_layout_boards = lakitu_boards | termina_boards

builder_incompatible_binaries_boards = frozenset([
    'grunt',
    'zork',
])
