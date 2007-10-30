/**************************************************************************
 *
 * Copyright (c) 2006-2007 Tungsten Graphics, Inc., Cedar Park, TX., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellström <thomas-at-tungstengraphics-dot-com>
 */

#include "drmP.h"

int drm_add_user_object(struct drm_file * priv, struct drm_user_object * item,
			int shareable)
{
	struct drm_device *dev = priv->head->dev;
	int ret;

	DRM_ASSERT_LOCKED(&dev->struct_mutex);

	/* The refcount will be bumped to 1 when we add the ref object below. */
	atomic_set(&item->refcount, 0);
	item->shareable = shareable;
	item->owner = priv;

	ret = drm_ht_just_insert_please(&dev->object_hash, &item->hash,
					(unsigned long)item, 32, 0, 0);
	if (ret)
		return ret;

	ret = drm_add_ref_object(priv, item, _DRM_REF_USE);
	if (ret)
		ret = drm_ht_remove_item(&dev->object_hash, &item->hash);

	return ret;
}
EXPORT_SYMBOL(drm_add_user_object);

struct drm_user_object *drm_lookup_user_object(struct drm_file * priv, uint32_t key)
{
	struct drm_device *dev = priv->head->dev;
	struct drm_hash_item *hash;
	int ret;
	struct drm_user_object *item;

	DRM_ASSERT_LOCKED(&dev->struct_mutex);

	ret = drm_ht_find_item(&dev->object_hash, key, &hash);
	if (ret) {
		return NULL;
	}
	item = drm_hash_entry(hash, struct drm_user_object, hash);

	if (priv != item->owner) {
		struct drm_open_hash *ht = &priv->refd_object_hash[_DRM_REF_USE];
		ret = drm_ht_find_item(ht, (unsigned long)item, &hash);
		if (ret) {
			DRM_ERROR("Object not registered for usage\n");
			return NULL;
		}
	}
	return item;
}
EXPORT_SYMBOL(drm_lookup_user_object);

static void drm_deref_user_object(struct drm_file * priv, struct drm_user_object * item)
{
	struct drm_device *dev = priv->head->dev;
	int ret;

	if (atomic_dec_and_test(&item->refcount)) {
		ret = drm_ht_remove_item(&dev->object_hash, &item->hash);
		BUG_ON(ret);
		item->remove(priv, item);
	}
}

static int drm_object_ref_action(struct drm_file * priv, struct drm_user_object * ro,
				 enum drm_ref_type action)
{
	int ret = 0;

	switch (action) {
	case _DRM_REF_USE:
		atomic_inc(&ro->refcount);
		break;
	default:
		if (!ro->ref_struct_locked) {
			break;
		} else {
			ro->ref_struct_locked(priv, ro, action);
		}
	}
	return ret;
}

int drm_add_ref_object(struct drm_file * priv, struct drm_user_object * referenced_object,
		       enum drm_ref_type ref_action)
{
	int ret = 0;
	struct drm_ref_object *item;
	struct drm_open_hash *ht = &priv->refd_object_hash[ref_action];

	DRM_ASSERT_LOCKED(&priv->head->dev->struct_mutex);
	if (!referenced_object->shareable && priv != referenced_object->owner) {
		DRM_ERROR("Not allowed to reference this object\n");
		return -EINVAL;
	}

	/*
	 * If this is not a usage reference, Check that usage has been registered
	 * first. Otherwise strange things may happen on destruction.
	 */

	if ((ref_action != _DRM_REF_USE) && priv != referenced_object->owner) {
		item =
		    drm_lookup_ref_object(priv, referenced_object,
					  _DRM_REF_USE);
		if (!item) {
			DRM_ERROR
			    ("Object not registered for usage by this client\n");
			return -EINVAL;
		}
	}

	if (NULL !=
	    (item =
	     drm_lookup_ref_object(priv, referenced_object, ref_action))) {
		atomic_inc(&item->refcount);
		return drm_object_ref_action(priv, referenced_object,
					     ref_action);
	}

	item = drm_ctl_calloc(1, sizeof(*item), DRM_MEM_OBJECTS);
	if (item == NULL) {
		DRM_ERROR("Could not allocate reference object\n");
		return -ENOMEM;
	}

	atomic_set(&item->refcount, 1);
	item->hash.key = (unsigned long)referenced_object;
	ret = drm_ht_insert_item(ht, &item->hash);
	item->unref_action = ref_action;

	if (ret)
		goto out;

	list_add(&item->list, &priv->refd_objects);
	ret = drm_object_ref_action(priv, referenced_object, ref_action);
      out:
	return ret;
}

struct drm_ref_object *drm_lookup_ref_object(struct drm_file * priv,
					struct drm_user_object * referenced_object,
					enum drm_ref_type ref_action)
{
	struct drm_hash_item *hash;
	int ret;

	DRM_ASSERT_LOCKED(&priv->head->dev->struct_mutex);
	ret = drm_ht_find_item(&priv->refd_object_hash[ref_action],
			       (unsigned long)referenced_object, &hash);
	if (ret)
		return NULL;

	return drm_hash_entry(hash, struct drm_ref_object, hash);
}
EXPORT_SYMBOL(drm_lookup_ref_object);

static void drm_remove_other_references(struct drm_file * priv,
					struct drm_user_object * ro)
{
	int i;
	struct drm_open_hash *ht;
	struct drm_hash_item *hash;
	struct drm_ref_object *item;

	for (i = _DRM_REF_USE + 1; i < _DRM_NO_REF_TYPES; ++i) {
		ht = &priv->refd_object_hash[i];
		while (!drm_ht_find_item(ht, (unsigned long)ro, &hash)) {
			item = drm_hash_entry(hash, struct drm_ref_object, hash);
			drm_remove_ref_object(priv, item);
		}
	}
}

void drm_remove_ref_object(struct drm_file * priv, struct drm_ref_object * item)
{
	int ret;
	struct drm_user_object *user_object = (struct drm_user_object *) item->hash.key;
	struct drm_open_hash *ht = &priv->refd_object_hash[item->unref_action];
	enum drm_ref_type unref_action;

	DRM_ASSERT_LOCKED(&priv->head->dev->struct_mutex);
	unref_action = item->unref_action;
	if (atomic_dec_and_test(&item->refcount)) {
		ret = drm_ht_remove_item(ht, &item->hash);
		BUG_ON(ret);
		list_del_init(&item->list);
		if (unref_action == _DRM_REF_USE)
			drm_remove_other_references(priv, user_object);
		drm_ctl_free(item, sizeof(*item), DRM_MEM_OBJECTS);
	}

	switch (unref_action) {
	case _DRM_REF_USE:
		drm_deref_user_object(priv, user_object);
		break;
	default:
		BUG_ON(!user_object->unref);
		user_object->unref(priv, user_object, unref_action);
		break;
	}

}

int drm_user_object_ref(struct drm_file * priv, uint32_t user_token,
			enum drm_object_type type, struct drm_user_object ** object)
{
	struct drm_device *dev = priv->head->dev;
	struct drm_user_object *uo;
	struct drm_hash_item *hash;
	int ret;

	mutex_lock(&dev->struct_mutex);
	ret = drm_ht_find_item(&dev->object_hash, user_token, &hash);
	if (ret) {
		DRM_ERROR("Could not find user object to reference.\n");
		goto out_err;
	}
	uo = drm_hash_entry(hash, struct drm_user_object, hash);
	if (uo->type != type) {
		ret = -EINVAL;
		goto out_err;
	}
	ret = drm_add_ref_object(priv, uo, _DRM_REF_USE);
	if (ret)
		goto out_err;
	mutex_unlock(&dev->struct_mutex);
	*object = uo;
	return 0;
      out_err:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

int drm_user_object_unref(struct drm_file * priv, uint32_t user_token,
			  enum drm_object_type type)
{
	struct drm_device *dev = priv->head->dev;
	struct drm_user_object *uo;
	struct drm_ref_object *ro;
	int ret;

	mutex_lock(&dev->struct_mutex);
	uo = drm_lookup_user_object(priv, user_token);
	if (!uo || (uo->type != type)) {
		ret = -EINVAL;
		goto out_err;
	}
	ro = drm_lookup_ref_object(priv, uo, _DRM_REF_USE);
	if (!ro) {
		ret = -EINVAL;
		goto out_err;
	}
	drm_remove_ref_object(priv, ro);
	mutex_unlock(&dev->struct_mutex);
	return 0;
      out_err:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}
