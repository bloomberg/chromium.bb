
noinst_DATA = xml/index.xml
dist_noinst_DATA = wayland.doxygen.in

scanned_src_files_client = 				\
	$(top_srcdir)/src/wayland-client.c	\
	$(top_srcdir)/src/wayland-client.h

scanned_src_files_server = 				\
	$(top_srcdir)/src/wayland-server.c	\
	$(top_srcdir)/src/wayland-server.h

client_MANPAGES = \
	man/man3/wl_display.3 \
	man/man3/wl_display_connect.3 \
	man/man3/wl_display_connect_to_fd.3 \
	man/man3/wl_display_create_queue.3 \
	man/man3/wl_display_disconnect.3 \
	man/man3/wl_display_dispatch.3 \
	man/man3/wl_display_dispatch_pending.3 \
	man/man3/wl_display_dispatch_queue.3 \
	man/man3/wl_display_dispatch_queue_pending.3 \
	man/man3/wl_display_flush.3 \
	man/man3/wl_display_get_error.3 \
	man/man3/wl_display_get_fd.3 \
	man/man3/wl_display_roundtrip.3 \
	man/man3/wl_event_queue.3 \
	man/man3/wl_event_queue_destroy.3 \
	man/man3/wl_proxy.3 \
	man/man3/wl_proxy_add_listener.3 \
	man/man3/wl_proxy_create.3 \
	man/man3/wl_proxy_destroy.3 \
	man/man3/wl_proxy_get_class.3 \
	man/man3/wl_proxy_get_id.3 \
	man/man3/wl_proxy_get_user_data.3 \
	man/man3/wl_proxy_marshal.3 \
	man/man3/wl_proxy_set_queue.3 \
	man/man3/wl_proxy_set_user_data.3

server_MANPAGES = \
	man/man3/wl_buffer.3 \
	man/man3/wl_client.3 \
	man/man3/wl_data_offer.3 \
	man/man3/wl_data_source.3 \
	man/man3/wl_global.3 \
	man/man3/wl_keyboard.3 \
	man/man3/wl_keyboard_grab.3 \
	man/man3/wl_keyboard_grab_interface.3 \
	man/man3/wl_listener.3 \
	man/man3/wl_pointer.3 \
	man/man3/wl_pointer_grab.3 \
	man/man3/wl_pointer_grab_interface.3 \
	man/man3/wl_resource.3 \
	man/man3/wl_seat.3 \
	man/man3/wl_signal.3 \
	man/man3/wl_socket.3 \
	man/man3/wl_surface.3 \
	man/man3/wl_touch.3 \
	man/man3/wl_touch_grab.3 \
	man/man3/wl_touch_grab_interface.3

man3_MANS= $(client_MANPAGES) $(server_MANPAGES)

xml/index.xml: $(scanned_src_files_client) wayland.doxygen
	doxygen wayland.doxygen

doxygen.man: $(scanned_src_files_client) $(scanned_src_files_server)
	(cat wayland.doxygen; \
          echo "GENERATE_XML=NO"; \
          echo "GENERATE_MAN=YES"; \
          echo "MAN_OUTPUT=man"; \
          echo "JAVADOC_AUTOBRIEF=NO"; \
          echo "INPUT= $^"; \
          ) | doxygen -

$(client_MANPAGES) $(server_MANPAGES): doxygen.man

clean-local:
	rm -rf xml/
	rm -rf man/
