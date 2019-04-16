deps_libweston = [
	dep_wayland_server,
	dep_pixman,
	dep_libm,
	dep_libdl,
	dep_libdrm_headers,
	dep_xkbcommon,
]
srcs_libweston = [
	git_version_h,
	'animation.c',
	'bindings.c',
	'clipboard.c',
	'compositor.c',
	'data-device.c',
	'input.c',
	'linux-dmabuf.c',
	'linux-explicit-synchronization.c',
	'linux-sync-file.c',
	'log.c',
	'noop-renderer.c',
	'pixel-formats.c',
	'pixman-renderer.c',
	'plugin-registry.c',
	'screenshooter.c',
	'timeline.c',
	'touch-calibration.c',
	'weston-debug.c',
	'zoom.c',
	'../shared/matrix.c',
	linux_dmabuf_unstable_v1_protocol_c,
	linux_dmabuf_unstable_v1_server_protocol_h,
	linux_explicit_synchronization_unstable_v1_protocol_c,
	linux_explicit_synchronization_unstable_v1_server_protocol_h,
	input_method_unstable_v1_protocol_c,
	input_method_unstable_v1_server_protocol_h,
	input_timestamps_unstable_v1_protocol_c,
	input_timestamps_unstable_v1_server_protocol_h,
	presentation_time_protocol_c,
	presentation_time_server_protocol_h,
	pointer_constraints_unstable_v1_protocol_c,
	pointer_constraints_unstable_v1_server_protocol_h,
	relative_pointer_unstable_v1_protocol_c,
	relative_pointer_unstable_v1_server_protocol_h,
	weston_screenshooter_protocol_c,
	weston_screenshooter_server_protocol_h,
	text_cursor_position_protocol_c,
	text_cursor_position_server_protocol_h,
	text_input_unstable_v1_protocol_c,
	text_input_unstable_v1_server_protocol_h,
	weston_touch_calibration_protocol_c,
	weston_touch_calibration_server_protocol_h,
	viewporter_protocol_c,
	viewporter_server_protocol_h,
	weston_debug_protocol_c,
	weston_debug_server_protocol_h,
]

install_headers(
	'compositor.h',
	'plugin-registry.h',
	'timeline-object.h',
	'windowed-output-api.h',
	'../shared/config-parser.h',
	'../shared/matrix.h',
	'../shared/zalloc.h',
	subdir: dir_include_libweston
)

if get_option('renderer-gl')
	dep_egl = dependency('egl', required: false)
	if not dep_egl.found()
		error('libweston + gl-renderer requires egl which was not found. Or, you can use \'-Drenderer-gl=false\'.')
	endif
	deps_libweston += dep_egl
endif

lib_weston = shared_library(
	'weston-@0@'.format(libweston_major),
	srcs_libweston,
	include_directories: include_directories('..', '../shared'),
	link_args: [ '-Wl,-export-dynamic' ],
	install: true,
	version: '0.0.@0@'.format(libweston_revision),
	link_whole: lib_libshared,
	dependencies: deps_libweston
)

dep_libweston = declare_dependency(
	link_with: lib_weston,
	include_directories: include_directories('.'),
	dependencies: deps_libweston
)

pkgconfig.generate(
	lib_weston,
	filebase: 'libweston-@0@'.format(libweston_major),
	name: 'libweston API',
	version: version_weston,
	description: 'Header files for libweston compositors development',
	requires_private: [ dep_wayland_server, dep_pixman, dep_xkbcommon ],
	subdirs: dir_include_libweston
)

pkgconfig.generate(
	filebase: 'libweston-@0@-protocols'.format(libweston_major),
	name: 'libWeston Protocols',
	version: version_weston,
	description: 'libWeston protocol files',
	variables: [
		'datarootdir=' + join_paths('${prefix}', get_option('datadir')),
		'pkgdatadir=' + join_paths('${pc_sysrootdir}${datarootdir}', dir_protocol_libweston)
	],
	install_dir: dir_data_pc
)

srcs_session_helper = [
	'launcher-direct.c',
	'launcher-util.c',
	'launcher-weston-launch.c',
]
deps_session_helper = [
	# for compositor.h needing pixman.h
	dep_pixman.partial_dependency(compile_args: true),
	# for compositor.h needing xkbcommon.h
	dep_xkbcommon.partial_dependency(compile_args: true),
	# for compositor.h needing wayland-server.h
	dep_wayland_server.partial_dependency(compile_args: true)
]

if get_option('backend-drm')
	deps_session_helper += dep_libdrm
endif

systemd_dep = dependency('', required: false)
if get_option('launcher-logind')
	systemd_dep = dependency('libsystemd', version: '>= 209', required: false)
	if systemd_dep.found()
		config_h.set('HAVE_SYSTEMD_LOGIN_209', '1')
	else
		systemd_dep = dependency('libsystemd-login', version: '>= 198', required: false)
		if not systemd_dep.found()
			error('logind support requires libsystemd or libsystemd-login but neither was found. Or, you can use \'-Dlauncher-logind=false\'')
		endif
	endif

	dbus_dep = dependency('dbus-1', version: '>= 1.6', required: false)
	if not dbus_dep.found()
		error('logind support requires dbus-1 >= 1.6 which was not found. Or, you can use \'-Dlauncher-logind=false\'')
	endif

	config_h.set('HAVE_DBUS', '1')
	config_h.set('HAVE_SYSTEMD_LOGIN', '1')

	srcs_session_helper += [
		'dbus.c',
		'launcher-logind.c',
	]
	deps_session_helper += [
		dbus_dep,
		systemd_dep,
	]
endif

lib_session_helper = static_library(
	'session-helper',
	srcs_session_helper,
	include_directories: include_directories('..', '../shared'),
	dependencies: deps_session_helper,
	install: false
)
dep_session_helper = declare_dependency(link_with: lib_session_helper)


if get_option('backend-drm')
	config_h.set('BUILD_DRM_COMPOSITOR', '1')

	srcs_drm = [
		'compositor-drm.c',
		'libbacklight.c',
		'libinput-device.c',
		'libinput-seat.c',
		linux_dmabuf_unstable_v1_protocol_c,
		linux_dmabuf_unstable_v1_server_protocol_h,
		presentation_time_server_protocol_h,
	]

	deps_drm = [
		dep_libweston,
		dep_session_helper,
		dep_libdrm,
		dep_libinput,
		dependency('libudev', version: '>= 136'),
	]

	# XXX: Actually let DRM-backend build without GBM, it really should
	if true # get_option('renderer-gl')
		dep_gbm = dependency('gbm', required: false)
		if not dep_gbm.found()
			error('drm-backend requires gbm which was not found. Or, you can use \'-Dbackend-drm=false\'.')
		endif
		if dep_gbm.version().version_compare('>= 17.1')
			config_h.set('HAVE_GBM_MODIFIERS', '1')
		endif
		if dep_gbm.version().version_compare('>= 17.2')
			config_h.set('HAVE_GBM_FD_IMPORT', '1')
		endif
		deps_drm += dep_gbm
	endif

	if get_option('backend-drm-screencast-vaapi')
		foreach name : [ 'libva', 'libva-drm' ]
			d = dependency(name, version: '>= 0.34.0', required: false)
			if not d.found()
				error('VA-API recorder requires @0@ >= 0.34.0 which was not found. Or, you can use \'-Dbackend-drm-screencast-vaapi=false\'.'.format(name))
			endif
			deps_drm += d
		endforeach

		srcs_drm += 'vaapi-recorder.c'
		deps_drm += dependency('threads')
		config_h.set('BUILD_VAAPI_RECORDER', '1')
	endif

	if dep_libdrm.version().version_compare('>= 2.4.71')
		config_h.set('HAVE_DRM_ADDFB2_MODIFIERS', '1')
	endif

	if dep_libdrm.version().version_compare('>= 2.4.78')
		config_h.set('HAVE_DRM_ATOMIC', '1')
	endif

	if dep_libdrm.version().version_compare('>= 2.4.83')
		config_h.set('HAVE_DRM_FORMATS_BLOB', '1')
	endif

	plugin_drm = shared_library(
		'drm-backend',
		srcs_drm,
		include_directories: include_directories('..', '../shared'),
		dependencies: deps_drm,
		name_prefix: '',
		install: true,
		install_dir: dir_module_libweston
	)
	env_modmap += 'drm-backend.so=@0@;'.format(plugin_drm.full_path())

	install_headers('compositor-drm.h', subdir: dir_include_libweston)
endif


if get_option('backend-headless')
	config_h.set('BUILD_HEADLESS_COMPOSITOR', '1')

	srcs_headless = [
		'compositor-headless.c',
		presentation_time_server_protocol_h,
	]
	plugin_headless = shared_library(
		'headless-backend',
		srcs_headless,
		include_directories: include_directories('..', '../shared'),
		dependencies: dep_libweston,
		name_prefix: '',
		install: true,
		install_dir: dir_module_libweston,
	)
	env_modmap += 'headless-backend.so=@0@;'.format(plugin_headless.full_path())
	install_headers('compositor-headless.h', subdir: dir_include_libweston)
endif


if get_option('backend-rdp')
	config_h.set('BUILD_RDP_COMPOSITOR', '1')

	dep_frdp = dependency('freerdp2', version: '>= 2.0.0', required: false)
	if not dep_frdp.found()
		error('RDP-backend requires freerdp2 which was not found. Or, you can use \'-Dbackend-rdp=false\'.')
	endif

	if cc.has_header('freerdp/version.h', dependencies: dep_frdp)
		config_h.set('HAVE_FREERDP_VERSION_H', '1')
	endif

	if cc.has_member(
		'SURFACE_BITS_COMMAND', 'bmp',
		dependencies : dep_frdp,
		prefix : '#include <freerdp/update.h>'
	)
		config_h.set('HAVE_SURFACE_BITS_BMP', '1')
	endif

	deps_rdp = [
		dep_libweston,
		dep_frdp,
	]
	plugin_rdp = shared_library(
		'rdp-backend',
		'compositor-rdp.c',
		include_directories: include_directories('..', '../shared'),
		dependencies: deps_rdp,
		name_prefix: '',
		install: true,
		install_dir: dir_module_libweston
	)
	env_modmap += 'rdp-backend.so=@0@;'.format(plugin_rdp.full_path())
	install_headers('compositor-rdp.h', subdir: dir_include_libweston)
endif


if get_option('backend-wayland')
	config_h.set('BUILD_WAYLAND_COMPOSITOR', '1')

	srcs_wlwl = [
		'compositor-wayland.c',
		fullscreen_shell_unstable_v1_client_protocol_h,
		fullscreen_shell_unstable_v1_protocol_c,
		presentation_time_protocol_c,
		presentation_time_server_protocol_h,
		xdg_shell_server_protocol_h,
		xdg_shell_protocol_c,
	]

	deps_wlwl = [
		dependency('wayland-client'),
		dependency('wayland-cursor'),
		dep_pixman,
		dep_libweston,
		dep_lib_cairo_shared,
	]

	if get_option('renderer-gl')
		d = dependency('wayland-egl', required: false)
		if not d.found()
			error('wayland-backend + gl-renderer requires wayland-egl which was not found. Or, you can use \'-Dbackend-wayland=false\' or \'-Drenderer-gl=false\'.')
		endif
		deps_wlwl += d
	endif

	plugin_wlwl = shared_library(
		'wayland-backend',
		srcs_wlwl,
		include_directories: include_directories('..', '../shared'),
		dependencies: deps_wlwl,
		name_prefix: '',
		install: true,
		install_dir: dir_module_libweston
	)
	env_modmap += 'wayland-backend.so=@0@;'.format(plugin_wlwl.full_path())
	install_headers('compositor-wayland.h', subdir: dir_include_libweston)
endif


if get_option('backend-x11')
	config_h.set('BUILD_X11_COMPOSITOR', '1')

	srcs_x11 = [
		'compositor-x11.c',
		presentation_time_server_protocol_h,
	]

	dep_x11_xcb = dependency('xcb', version: '>= 1.8', required: false)
	if not dep_x11_xcb.found()
		error('x11-backend requires xcb >= 1.8 which was not found. Or, you can use \'-Dbackend-x11=false\'.')
	endif

	deps_x11 = [
		dep_libweston,
		dep_x11_xcb,
		dep_lib_cairo_shared,
		dep_pixman,
	]

	foreach name : [ 'xcb-shm', 'x11', 'x11-xcb' ]
		d = dependency(name, required: false)
		if not d.found()
			error('x11-backend requires @0@ which was not found. Or, you can use \'-Dbackend-x11=false\'.'.format(name))
		endif
		deps_x11 += d
	endforeach

	dep_xcb_xkb = dependency('xcb-xkb', version: '>= 1.9', required: false)
	if dep_xcb_xkb.found()
		deps_x11 += dep_xcb_xkb
		config_h.set('HAVE_XCB_XKB', '1')
	endif

	if get_option('renderer-gl')
		if not dep_egl.found()
			error('x11-backend + gl-renderer requires egl which was not found. Or, you can use \'-Dbackend-x11=false\' or \'-Drenderer-gl=false\'.')
		endif
		deps_x11 += dep_egl
	endif

	plugin_x11 = shared_library(
		'x11-backend',
		srcs_x11,
		include_directories: include_directories('..', '../shared'),
		dependencies: deps_x11,
		name_prefix: '',
		install: true,
		install_dir: dir_module_libweston
	)
	env_modmap += 'x11-backend.so=@0@;'.format(plugin_x11.full_path())

	install_headers('compositor-x11.h', subdir: dir_include_libweston)
endif


if get_option('backend-fbdev')
	config_h.set('BUILD_FBDEV_COMPOSITOR', '1')

	srcs_fbdev = [
		'compositor-fbdev.c',
		'libinput-device.c',
		'libinput-seat.c',
		presentation_time_server_protocol_h,
	]

	deps_fbdev = [
		dep_libweston,
		dep_session_helper,
		dep_libinput,
		dependency('libudev', version: '>= 136'),
	]

	plugin_fbdev = shared_library(
		'fbdev-backend',
		srcs_fbdev,
		include_directories: include_directories('..', '../shared'),
		dependencies: deps_fbdev,
		name_prefix: '',
		install: true,
		install_dir: dir_module_libweston
	)
	env_modmap += 'fbdev-backend.so=@0@;'.format(plugin_fbdev.full_path())

	install_headers('compositor-fbdev.h', subdir: dir_include_libweston)
endif


if get_option('renderer-gl')
	config_h.set('ENABLE_EGL', '1')

	srcs_renderer_gl = [
		'gl-renderer.c',
		'linux-sync-file.c',
		'vertex-clipping.c',
		'../shared/matrix.c',
		linux_dmabuf_unstable_v1_protocol_c,
		linux_dmabuf_unstable_v1_server_protocol_h,
	]

	deps_renderer_gl = [
		dep_pixman,
		dep_libweston,
		dep_libdrm_headers,
	]

	foreach name : [ 'egl', 'glesv2' ]
		d = dependency(name, required: false)
		if not d.found()
			error('gl-renderer requires @0@ which was not found. Or, you can use \'-Drenderer-gl=false\'.'.format(name))
		endif
		deps_renderer_gl += d
	endforeach

	plugin_gl = shared_library(
		'gl-renderer',
		srcs_renderer_gl,
		include_directories: include_directories('..', '../shared'),
		dependencies: deps_renderer_gl,
		name_prefix: '',
		install: true,
		install_dir: dir_module_libweston
	)
	env_modmap += 'gl-renderer.so=@0@;'.format(plugin_gl.full_path())
endif

if get_option('weston-launch')
	dep_pam = cc.find_library('pam')

	if not cc.has_function('pam_open_session', dependencies: dep_pam)
		error('pam_open_session not found for weston-launch')
	endif

	executable(
		'weston-launch',
		'weston-launch.c',
		dependencies: [dep_pam, systemd_dep, dep_libdrm],
		include_directories: include_directories('..'),
		install: true
	)

	meson.add_install_script('echo', 'REMINDER: You are installing weston-launch, please make it setuid-root.')
endif
