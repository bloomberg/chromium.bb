#include <stdlib.h>
#include "wayland.h"

int wl_hash_insert(struct wl_hash *hash, struct wl_object *object)
{
	struct wl_object **objects;
	uint32_t alloc;

	if (hash->count == hash->alloc) {
		if (hash->alloc == 0)
			alloc = 16;
		else
			alloc = hash->alloc * 2;
		objects = realloc(hash->objects, alloc * sizeof *objects);
		if (objects == NULL)
			return -1;

		hash->objects = objects;
		hash->alloc = alloc;
	}

	hash->objects[hash->count] = object;
	hash->count++;

	return 0;
}

struct wl_object *
wl_hash_lookup(struct wl_hash *hash, uint32_t id)
{
	int i;

	for (i = 0; i < hash->count; i++) {
		if (hash->objects[i]->id == id)
			return hash->objects[i];
	}

	return NULL;
}

void
wl_hash_delete(struct wl_hash *hash, struct wl_object *object)
{
	/* writeme */
}
