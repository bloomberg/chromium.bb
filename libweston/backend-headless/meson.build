if not get_option('backend-headless')
	subdir_done()
endif

config_h.set('BUILD_HEADLESS_COMPOSITOR', '1')

srcs_headless = [
	'headless.c',
	presentation_time_server_protocol_h,
]
plugin_headless = shared_library(
	'headless-backend',
	srcs_headless,
	include_directories: include_directories('../..', '../../shared'),
	dependencies: dep_libweston,
	name_prefix: '',
	install: true,
	install_dir: dir_module_libweston,
)
env_modmap += 'headless-backend.so=@0@;'.format(plugin_headless.full_path())
install_headers(backend_headless_h, subdir: dir_include_libweston_install)
