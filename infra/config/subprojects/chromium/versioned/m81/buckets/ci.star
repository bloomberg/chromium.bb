load('//lib/builders.star', 'builder_name', 'cpu', 'goma', 'os')
load('//lib/ci.star', 'ci')
# Load this using relative path so that the load statement doesn't
# need to be changed when making a new milestone
load('../vars.star', 'vars')


ci.set_defaults(
    vars,
    bucketed_triggers = True,
    main_console_view = vars.main_console_name,
)

ci.declare_bucket(vars)


# Builders are sorted first lexicographically by the function used to define
# them, then lexicographically by their name


ci.android_builder(
    name = 'Android arm Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'builder|arm',
        short_name = '32',
    ),
    execution_timeout = 4 * time.hour,
)

ci.android_builder(
    name = 'Cast Android (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'on_cq',
        short_name = 'cst',
    ),
)

ci.android_builder(
    name = 'android-cronet-arm-rel',
    console_view_entry = ci.console_view_entry(
        category = 'cronet|arm',
        short_name = 'rel',
    ),
    notifies = ['cronet'],
)

ci.android_builder(
    name = 'android-cronet-kitkat-arm-rel',
    console_view_entry = ci.console_view_entry(
        category = 'cronet|test',
        short_name = 'k',
    ),
    notifies = ['cronet'],
    triggered_by = [builder_name('android-cronet-arm-rel')],
)

ci.android_builder(
    name = 'android-cronet-lollipop-arm-rel',
    console_view_entry = ci.console_view_entry(
        category = 'cronet|test',
        short_name = 'l',
    ),
    notifies = ['cronet'],
    triggered_by = [builder_name('android-cronet-arm-rel')],
)

ci.android_builder(
    name = 'android-kitkat-arm-rel',
    console_view_entry = ci.console_view_entry(
        category = 'on_cq',
        short_name = 'K',
    ),
)

ci.android_builder(
    name = 'android-lollipop-arm-rel',
    console_view_entry = ci.console_view_entry(
        category = 'on_cq',
        short_name = 'L',
    ),
)

ci.android_builder(
    name = 'android-marshmallow-arm64-rel',
    console_view_entry = ci.console_view_entry(
        category = 'on_cq',
        short_name = 'M',
    ),
)


ci.chromiumos_builder(
    name = 'chromeos-amd64-generic-rel',
    console_view_entry = ci.console_view_entry(
        category = 'simple|release|x64',
        short_name = 'rel',
    ),
)

ci.chromiumos_builder(
    name = 'chromeos-arm-generic-rel',
    console_view_entry = ci.console_view_entry(
        category = 'simple|release',
        short_name = 'arm',
    ),
)

ci.chromiumos_builder(
    name = 'linux-chromeos-dbg',
    console_view_entry = ci.console_view_entry(
        category = 'default',
        short_name = 'dbg',
    ),
)

ci.chromiumos_builder(
    name = 'linux-chromeos-rel',
    console_view_entry = ci.console_view_entry(
        category = 'default',
        short_name = 'rel',
    ),
)


# This is launching & collecting entirely isolated tests.
# OS shouldn't matter.
ci.fyi_builder(
    name = 'mac-osxbeta-rel',
    console_view_entry = ci.console_view_entry(
        category = 'mac',
        short_name = 'beta',
    ),
    goma_backend = None,
    triggered_by = [builder_name('Mac Builder')],
)


ci.fyi_windows_builder(
    name = 'Win10 Tests x64 1803',
    console_view_entry = ci.console_view_entry(
        category = 'win10|1803',
    ),
    goma_backend = None,
    os = os.WINDOWS_10,
    triggered_by = [builder_name('Win x64 Builder')],
)


ci.gpu_builder(
    name = 'Android Release (Nexus 5X)',
    console_view_entry = ci.console_view_entry(
        category = 'Android',
    ),
)

ci.gpu_builder(
    name = 'GPU Linux Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Linux',
    ),
)

ci.gpu_builder(
    name = 'GPU Mac Builder',
    console_view_entry = ci.console_view_entry(
        category = 'Mac',
    ),
    cores = None,
    os = os.MAC_ANY,
)

ci.gpu_builder(
    name = 'GPU Win x64 Builder',
    builderless = True,
    console_view_entry = ci.console_view_entry(
        category = 'Windows',
    ),
    os = os.WINDOWS_ANY,
)


ci.gpu_thin_tester(
    name = 'Linux Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Linux',
    ),
    triggered_by = [builder_name('GPU Linux Builder')],
)

ci.gpu_thin_tester(
    name = 'Mac Release (Intel)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac',
    ),
    triggered_by = [builder_name('GPU Mac Builder')],
)

ci.gpu_thin_tester(
    name = 'Mac Retina Release (AMD)',
    console_view_entry = ci.console_view_entry(
        category = 'Mac',
    ),
    triggered_by = [builder_name('GPU Mac Builder')],
)

ci.gpu_thin_tester(
    name = 'Win10 x64 Release (NVIDIA)',
    console_view_entry = ci.console_view_entry(
        category = 'Windows',
    ),
    triggered_by = [builder_name('GPU Win x64 Builder')],
)


ci.linux_builder(
    name = 'Cast Linux',
    console_view_entry = ci.console_view_entry(
        category = 'cast',
        short_name = 'vid',
    ),
    goma_jobs = goma.jobs.J50,
)

ci.linux_builder(
    name = 'Fuchsia ARM64',
    console_view_entry = ci.console_view_entry(
        category = 'fuchsia|a64',
    ),
    notifies = ['cr-fuchsia'],
)

ci.linux_builder(
    name = 'Fuchsia x64',
    console_view_entry = ci.console_view_entry(
        category = 'fuchsia|x64',
        short_name = 'rel',
    ),
    notifies = ['cr-fuchsia'],
)

ci.linux_builder(
    name = 'Linux Builder',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'bld',
    ),
)

ci.linux_builder(
    name = 'Linux Tests',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'tst',
    ),
    goma_backend = None,
    triggered_by = [builder_name('Linux Builder')],
)

ci.linux_builder(
    name = 'linux-ozone-rel',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'ozo',
    ),
)

ci.linux_builder(
    name = 'Linux Ozone Tester (Headless)',
    console_view = 'chromium.fyi',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
        short_name = 'loh',
    ),
    goma_backend = None,
    triggered_by = [builder_name('linux-ozone-rel')],
)

ci.linux_builder(
    name = 'Linux Ozone Tester (Wayland)',
    console_view = 'chromium.fyi',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
        short_name = 'low',
    ),
    goma_backend = None,
    triggered_by = [builder_name('linux-ozone-rel')],
)

ci.linux_builder(
    name = 'Linux Ozone Tester (X11)',
    console_view = 'chromium.fyi',
    console_view_entry = ci.console_view_entry(
        category = 'linux',
        short_name = 'lox',
    ),
    goma_backend = None,
    triggered_by = [builder_name('linux-ozone-rel')],
)


ci.mac_builder(
    name = 'Mac Builder',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'bld',
    ),
    os = os.MAC_10_14,
)

ci.mac_builder(
    name = 'Mac Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'debug',
        short_name = 'bld',
    ),
    os = os.MAC_ANY,
)

ci.thin_tester(
    name = 'Mac10.10 Tests',
    mastername = 'chromium.mac',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = '10',
    ),
    triggered_by = [builder_name('Mac Builder')],
)

ci.thin_tester(
    name = 'Mac10.11 Tests',
    mastername = 'chromium.mac',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = '11',
    ),
    triggered_by = [builder_name('Mac Builder')],
)

ci.thin_tester(
    name = 'Mac10.12 Tests',
    mastername = 'chromium.mac',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = '12',
    ),
    triggered_by = [builder_name('Mac Builder')],
)

ci.thin_tester(
    name = 'Mac10.13 Tests',
    mastername = 'chromium.mac',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = '13',
    ),
    triggered_by = [builder_name('Mac Builder')],
)

ci.thin_tester(
    name = 'Mac10.14 Tests',
    mastername = 'chromium.mac',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = '14',
    ),
    triggered_by = [builder_name('Mac Builder')],
)

ci.thin_tester(
    name = 'Mac10.13 Tests (dbg)',
    mastername = 'chromium.mac',
    console_view_entry = ci.console_view_entry(
        category = 'debug',
        short_name = '13',
    ),
    triggered_by = [builder_name('Mac Builder (dbg)')],
)

ci.thin_tester(
    name = 'WebKit Mac10.13 (retina)',
    mastername = 'chromium.mac',
    console_view_entry = ci.console_view_entry(
        category = 'release',
        short_name = 'ret',
    ),
    triggered_by = [builder_name('Mac Builder')],
)


ci.mac_ios_builder(
    name = 'ios-simulator',
    console_view_entry = ci.console_view_entry(
        category = 'ios|default',
        short_name = 'sim',
    ),
    goma_backend = None,
)


ci.memory_builder(
    name = 'Linux ASan LSan Builder',
    console_view_entry = ci.console_view_entry(
        category = 'linux|asan lsan',
        short_name = 'bld',
    ),
    ssd = True,
)

ci.memory_builder(
    name = 'Linux ASan LSan Tests (1)',
    console_view_entry = ci.console_view_entry(
        category = 'linux|asan lsan',
        short_name = 'tst',
    ),
    triggered_by = [builder_name('Linux ASan LSan Builder')],
)

ci.memory_builder(
    name = 'Linux ASan Tests (sandboxed)',
    console_view_entry = ci.console_view_entry(
        category = 'linux|asan lsan',
        short_name = 'sbx',
    ),
    triggered_by = [builder_name('Linux ASan LSan Builder')],
)


ci.win_builder(
    name = 'Win7 Tests (dbg)(1)',
    console_view_entry = ci.console_view_entry(
        category = 'debug|tester',
        short_name = '7',
    ),
    os = os.WINDOWS_7,
    triggered_by = [builder_name('Win Builder (dbg)')],
)

ci.win_builder(
    name = 'Win 7 Tests x64 (1)',
    console_view_entry = ci.console_view_entry(
        category = 'release|tester',
        short_name = '64',
    ),
    os = os.WINDOWS_7,
    triggered_by = [builder_name('Win x64 Builder')],
)

ci.win_builder(
    name = 'Win Builder (dbg)',
    console_view_entry = ci.console_view_entry(
        category = 'debug|builder',
        short_name = '32',
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = 'Win x64 Builder',
    console_view_entry = ci.console_view_entry(
        category = 'release|builder',
        short_name = '64',
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
)

ci.win_builder(
    name = 'Win10 Tests x64',
    console_view_entry = ci.console_view_entry(
        category = 'release|tester',
        short_name = 'w10',
    ),
    triggered_by = [builder_name('Win x64 Builder')],
)
