
struct egl_input_device;
void
egl_device_get_position(struct egl_input_device *device, int32_t *x, int32_t *y);
void
notify_motion(struct egl_input_device *device, int x, int y);
void
notify_button(struct egl_input_device *device, int32_t button, int32_t state);
void
notify_key(struct egl_input_device *device, uint32_t key, uint32_t state);

struct evdev_input_device *
evdev_input_device_create(struct egl_input_device *device,
			  struct wl_display *display, const char *path);

